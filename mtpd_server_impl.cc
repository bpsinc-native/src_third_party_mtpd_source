// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/mtpd_server_impl.h"

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>

namespace mtpd {

namespace {

const char kInvalidHandleErrorMessage[] = "Invalid handle ";

void SetInvalidHandleError(const std::string& handle, DBus::Error* error) {
  std::string error_msg = kInvalidHandleErrorMessage + handle;
  error->set(kMtpdServiceError, error_msg.c_str());
}

template<typename ReturnType>
ReturnType InvalidHandle(const std::string& handle, DBus::Error* error) {
  SetInvalidHandleError(handle, error);
  return ReturnType();
}


}  // namespace

MtpdServer::MtpdServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, kMtpdServicePath),
      device_manager_(this) {
}

MtpdServer::~MtpdServer() {
}

std::vector<std::string> MtpdServer::EnumerateStorage(DBus::Error& error) {
  return device_manager_.EnumerateStorage();
}

DBusMTPStorage MtpdServer::GetStorageInfo(const std::string& storageName,
                                          DBus::Error& error) {
  const StorageInfo* info = device_manager_.GetStorageInfo(storageName);
  return info ? info->ToDBusFormat() : DBusMTPStorage();
}

std::string MtpdServer::OpenStorage(const std::string& storageName,
                                    const std::string& mode,
                                    DBus::Error& error) {
  // TODO(thestig) Handle read-write and possibly append-only modes.
  if (mode != kReadOnlyMode) {
    std::string error_msg = "Cannot open " + storageName + " in mode: " + mode;
    error.set(kMtpdServiceError, error_msg.c_str());
    return std::string();
  }

  if (!device_manager_.HasStorage(storageName)) {
    std::string error_msg = "Cannot open unknown storage " + storageName;
    error.set(kMtpdServiceError, error_msg.c_str());
    return std::string();
  }

  std::string id;
  uint32 random_data[4];
  do {
    base::RandBytes(random_data, sizeof(random_data));
    id = base::HexEncode(random_data, sizeof(random_data));
  } while (ContainsKey(handle_map_, id));

  handle_map_.insert(std::make_pair(id, storageName));
  return id;
}

void MtpdServer::CloseStorage(const std::string& handle, DBus::Error& error) {
  if (handle_map_.erase(handle) == 0)
    SetInvalidHandleError(handle, &error);
}

DBusFileEntries MtpdServer::ReadDirectoryByPath(const std::string& handle,
                                                const std::string& filePath,
                                                DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<DBusFileEntries>(handle, &error);

  DBusFileEntries directory_listing;
  if (!device_manager_.ReadDirectoryByPath(storage_name,
                                           filePath,
                                           &directory_listing)) {
    error.set(kMtpdServiceError, "ReadDirectoryByPath failed");
    return DBusFileEntries();
  }
  return directory_listing;
}

DBusFileEntries MtpdServer::ReadDirectoryById(const std::string& handle,
                                              const uint32_t& fileId,
                                              DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<DBusFileEntries>(handle, &error);

  DBusFileEntries directory_listing;
  if (!device_manager_.ReadDirectoryById(storage_name,
                                         fileId,
                                         &directory_listing)) {
    error.set(kMtpdServiceError, "ReadDirectoryById failed");
    return DBusFileEntries();
  }
  return directory_listing;
}

std::vector<uint8_t> MtpdServer::ReadFileByPath(const std::string& handle,
                                                const std::string& filePath,
                                                DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<std::vector<uint8_t> >(handle, &error);

  std::vector<uint8_t> file_contents;
  if (!device_manager_.ReadFileByPath(storage_name, filePath, &file_contents)) {
    error.set(kMtpdServiceError, "ReadFileByPath failed");
    return std::vector<uint8_t>();
  }
  return file_contents;
}

std::vector<uint8_t> MtpdServer::ReadFileById(const std::string& handle,
                                              const uint32_t& fileId,
                                              DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<std::vector<uint8_t> >(handle, &error);

  std::vector<uint8_t> file_contents;
  if (!device_manager_.ReadFileById(storage_name, fileId, &file_contents)) {
    error.set(kMtpdServiceError, "ReadFileById failed");
    return std::vector<uint8_t>();
  }
  return file_contents;
}

DBusFileEntry MtpdServer::GetFileInfoByPath(const std::string& handle,
                                            const std::string& filePath,
                                            DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<DBusFileEntry>(handle, &error);

  DBusFileEntry entry;
  if (!device_manager_.GetFileInfoByPath(storage_name, filePath, &entry)) {
    error.set(kMtpdServiceError, "GetFileInfoByPath failed");
    return DBusFileEntry();
  }
  return entry;
}

DBusFileEntry MtpdServer::GetFileInfoById(const std::string& handle,
                                          const uint32_t& fileId,
                                          DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<DBusFileEntry>(handle, &error);

  DBusFileEntry entry;
  if (!device_manager_.GetFileInfoById(storage_name, fileId, &entry)) {
    error.set(kMtpdServiceError, "GetFileInfoById failed");
    return DBusFileEntry();
  }
  return entry;
}

bool MtpdServer::IsAlive(DBus::Error& error) {
  return true;
}

void MtpdServer::StorageAttached(const std::string& storage_name) {
  // Fire DBus signal.
  MTPStorageAttached(storage_name);
}

void MtpdServer::StorageDetached(const std::string& storage_name) {
  // Fire DBus signal.
  MTPStorageDetached(storage_name);
}

int MtpdServer::GetDeviceEventDescriptor() const {
  return device_manager_.GetDeviceEventDescriptor();
}

void MtpdServer::ProcessDeviceEvents() {
  device_manager_.ProcessDeviceEvents();
}

std::string MtpdServer::LookupHandle(const std::string& handle) {
  HandleMap::const_iterator it = handle_map_.find(handle);
  return (it == handle_map_.end()) ? std::string() : it->second;
}

}  // namespace mtpd
