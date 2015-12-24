//===----RTLs/hsa/src/rtl.cpp - Target RTLs Implementation -------- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// RTL for hsa machine
// github: guansong (zhang.guansong@gmail.com)
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <gelf.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "hsa.h"
#include "hsa_ext_finalize.h"

#include "omptarget.h"

/* Sync the limits with device, as we use static structure */
//#include "option.h"

#ifndef TARGET_NAME
#define TARGET_NAME HSA
#endif

#define GETNAME2(name) #name
#define GETNAME(name) GETNAME2(name)
#define DP(...) DEBUGP("Target " GETNAME(TARGET_NAME) " RTL",__VA_ARGS__)

#ifdef OMPTARGET_DEBUG
#define check(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
    /* fprintf(stderr, "[%s:%d] %s failed.\n", __FILE__, __LINE__, #msg);*/ \
    DP(#msg"failed \n") \
    /*assert(0);*/ \
} else { \
    /* fprintf(stderr, "[%s:%d] %s succeeded.\n", __FILE__, __LINE__, #msg); */ \
    DP(#msg" succeeded\n") \
}
#else
#define check(msg, status) \
  {}
#endif


/// Account the memory allocated per device
struct AllocMemEntryTy {
  unsigned TotalSize;
  std::vector<void*> Ptrs;

  AllocMemEntryTy() : TotalSize(0) {}
};

/// Keep entries table per device
struct FuncOrGblEntryTy {
  __tgt_target_table Table;
  std::vector<__tgt_offload_entry> Entries;
};

/// Class containing all the device information
class RTLDeviceInfoTy {
  std::vector<FuncOrGblEntryTy> FuncGblEntries;

  std::vector<AllocMemEntryTy> MemoryEntries;
  
public:
  int NumberOfDevices;

  //std::list<std::string> brigModules;

  std::vector<int> ThreadsPerGroup;
  std::vector<int> GroupsPerDevice;

  // Record memory pointer associated with device
  void addMemory(int32_t device_id, void *ptr, int size){
    assert( device_id < MemoryEntries.size() && "Unexpected device id!");
    AllocMemEntryTy &E = MemoryEntries[device_id];
    E.TotalSize += size;
    E.Ptrs.push_back(ptr);
  }

  // Return true if the pointer is associated with device
  bool findMemory(int32_t device_id, void *ptr){
    assert( device_id < MemoryEntries.size() && "Unexpected device id!");
    AllocMemEntryTy &E = MemoryEntries[device_id];

    return  std::find(E.Ptrs.begin(),E.Ptrs.end(),ptr) != E.Ptrs.end();
  }

  // Remove pointer from the dtaa memory map
  void deleteMemory(int32_t device_id, void *ptr){
    assert( device_id < MemoryEntries.size() && "Unexpected device id!");
    AllocMemEntryTy &E = MemoryEntries[device_id];

    std::vector<void*>::iterator it = std::find(E.Ptrs.begin(),E.Ptrs.end(),ptr);

    if(it != E.Ptrs.end()){
      E.Ptrs.erase(it);
    }
  }

  // Record entry point associated with device
  void addOffloadEntry(int32_t device_id, __tgt_offload_entry entry ){
    assert( device_id < FuncGblEntries.size() && "Unexpected device id!");
    FuncOrGblEntryTy &E = FuncGblEntries[device_id];

    E.Entries.push_back(entry);
  }

  // Return true if the entry is associated with device
  bool findOffloadEntry(int32_t device_id, void *addr){
    assert( device_id < FuncGblEntries.size() && "Unexpected device id!");
    FuncOrGblEntryTy &E = FuncGblEntries[device_id];

    for(unsigned i=0; i<E.Entries.size(); ++i){
      if(E.Entries[i].addr == addr)
        return true;
    }

    return false;
  }

  // Return the pointer to the target entries table
  __tgt_target_table *getOffloadEntriesTable(int device_id){
    assert( device_id < FuncGblEntries.size() && "Unexpected device id!");
    FuncOrGblEntryTy &E = FuncGblEntries[device_id];

    unsigned size = E.Entries.size();

    // Table is empty
    if(!size)
      return 0;

    __tgt_offload_entry *begin = &E.Entries[0];
    __tgt_offload_entry *end = &E.Entries[size-1];

    // Update table info according to the entries and return the pointer
    E.Table.EntriesBegin = begin;
    E.Table.EntriesEnd = ++end;

    return &E.Table;
  }

  // Clear entries table for a device
  void clearOffloadEntriesTable(int device_id){
    assert( device_id < FuncGblEntries.size() && "Unexpected device id!");
    FuncOrGblEntryTy &E = FuncGblEntries[device_id];
    E.Entries.clear();
    E.Table.EntriesBegin = E.Table.EntriesEnd = 0;
  }

  RTLDeviceInfoTy(int num_devices){
    DP ("Initializing HSA\n");
  }

  ~RTLDeviceInfoTy(){
    DP("De-initializing HSA.\n");
  }
};

// assume this is one for now;
// HSA runtime 1.0 seems only define one device for each type
#define NUMBER_OF_DEVICES 1

static RTLDeviceInfoTy DeviceInfo(NUMBER_OF_DEVICES);

#ifdef __cplusplus
extern "C" {
#endif

int __tgt_rtl_device_type(int device_id){

  if( device_id < DeviceInfo.NumberOfDevices)
    return 224; // EM_HSA 

  return 0;
}

int __tgt_rtl_number_of_devices(){
  return DeviceInfo.NumberOfDevices;
}

int32_t __tgt_rtl_init_device(int device_id){
  // this is per device id init
  DP("Initialize the device id: %d\n", device_id);
  return OFFLOAD_SUCCESS ;
}

__tgt_target_table *__tgt_rtl_load_binary(int32_t device_id, __tgt_device_image *image){
    DP("Loading binary!\n");
    __tgt_offload_entry *HostBegin = image->EntriesBegin;
    __tgt_offload_entry *HostEnd   = image->EntriesEnd;

    for( __tgt_offload_entry *e = HostBegin; e != HostEnd; ++e) {
        if( !e->addr ){
            // FIXME: Probably we should fail when something like this happen, the
            // host should have always something in the address to uniquely identify
            // the target region.
            DP("Analyzing host entry '<null>' (size = %lld)...\n",
                    (unsigned long long)e->size);

            __tgt_offload_entry entry = *e;
            DeviceInfo.addOffloadEntry(device_id, entry);
            continue;
        }

        if( e->size ){

            __tgt_offload_entry entry = *e;

            DP("Find globale var name: %s\n", e->name);
            continue;
        }
    }
    return DeviceInfo.getOffloadEntriesTable(device_id);
}

void *__tgt_rtl_data_alloc(int device_id, int size){
  DP("System Alloc data %d bytes.\n", size);
  return NULL;
}

int32_t __tgt_rtl_data_submit(int device_id, void *tgt_ptr, void *hst_ptr, int size){
  DP("System Submit data %d bytes.\n", size);
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_data_retrieve(int device_id, void *hst_ptr, void *tgt_ptr, int size){
  DP("System Retrieve data %d bytes.\n", size);
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_data_delete(int device_id, void* tgt_ptr){
  DP("System Delete data %p.\n", tgt_ptr);
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_run_target_team_region(int32_t device_id, 
    void *tgt_entry_ptr, void **tgt_args, int32_t arg_num,
    int32_t team_num, int32_t thread_limit){
  DP("Run Target Team Region\n");
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_run_target_region(int32_t device_id, void *tgt_entry_ptr,
  void **tgt_args, int32_t arg_num)
{
  // use one team and one thread
  // fix thread num
  int32_t team_num = 1;
  int32_t thread_limit = 0; // use default
  return __tgt_rtl_run_target_team_region(device_id,
    tgt_entry_ptr, tgt_args, arg_num, team_num, thread_limit);
}

#ifdef __cplusplus
}
#endif

