//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_win.h"

#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "util/logging.h"
#include <Windows.h>

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  if (tv != nullptr) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli = { ft.dwLowDateTime, ft.dwHighDateTime };
    uli.QuadPart -= 116444736000000000ull;
    tv->tv_sec = uli.QuadPart / 10000000;
    tv->tv_usec = (uli.QuadPart % 10000000 + 5) / 10;
  }
  return 0;
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
  return localtime_s(result, timep) == 0 ? result : nullptr;
}

int pthread_key_create(pthread_key_t *key, void(*destructor)(void*)) {
  // TODO: Register destructor to be called from DllMain
  return TlsAlloc();
}

void *pthread_getspecific(pthread_key_t key) {
  return TlsGetValue(key);
}

int pthread_setspecific(pthread_key_t key, const void *value) {
  return TlsSetValue(key, const_cast<void *>(value)) == FALSE ? 0 : GetLastError();
}

namespace rocksdb {
namespace port {

Mutex::Mutex(bool adaptive) {
  // ignore adaptive for non-linux platform
  InitializeCriticalSection(&cs_);
}

Mutex::~Mutex() {
  DeleteCriticalSection(&cs_);
}

void Mutex::Lock() {
  EnterCriticalSection(&cs_);
#ifndef NDEBUG
  locked_ = true;
#endif
}

void Mutex::Unlock() {
#ifndef NDEBUG
  locked_ = false;
#endif
  LeaveCriticalSection(&cs_);
}

void Mutex::AssertHeld() {
#ifndef NDEBUG
  assert(locked_);
#endif
}

CondVar::CondVar(Mutex* mu)
    : mu_(mu) {
  InitializeConditionVariable(&cv_);
}

CondVar::~CondVar() {
}

void CondVar::Wait() {
#ifndef NDEBUG
  mu_->locked_ = false;
#endif
  SleepConditionVariableCS(&cv_, &mu_->cs_, INFINITE);
#ifndef NDEBUG
  mu_->locked_ = true;
#endif
}

bool CondVar::TimedWait(uint64_t abs_time_us) {
#ifndef NDEBUG
  mu_->locked_ = false;
#endif
  BOOL success = SleepConditionVariableCS(&cv_, &mu_->cs_, static_cast<DWORD>(std::max(UINT32_MAX - 1ui64, abs_time_us / 1000)));
#ifndef NDEBUG
  mu_->locked_ = true;
#endif
  return success == FALSE && GetLastError() == ERROR_TIMEOUT;
}

void CondVar::Signal() {
  WakeConditionVariable(&cv_);
}

void CondVar::SignalAll() {
  WakeAllConditionVariable(&cv_);
}

RWMutex::RWMutex() {
  InitializeSRWLock(rw_);
}

RWMutex::~RWMutex() {
}

void RWMutex::ReadLock() {
  AcquireSRWLockShared(rw_);
}

void RWMutex::WriteLock() {
  AcquireSRWLockExclusive(rw_);
}

void RWMutex::ReadUnlock() { 
  ReleaseSRWLockShared(rw_);
}

void RWMutex::WriteUnlock() {
  ReleaseSRWLockExclusive(rw_);
}

BOOL CALLBACK RunInitializer(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *lpContext) {
  INIT_PROC initializer = static_cast<INIT_PROC>(Parameter);
  initializer();
  return TRUE;
}

void InitOnce(port::OnceType * once, void(*initializer)()) {
  InitOnceExecuteOnce(once, RunInitializer, initializer, NULL);
}


}  // namespace port
}  // namespace rocksdb
