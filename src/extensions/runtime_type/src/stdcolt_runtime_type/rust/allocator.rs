#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

use core::ffi::c_void;
use core::os::raw::{c_int, c_longlong};

pub type uint64_t = u64;
pub type uint32_t = u32;
pub type int32_t = i32;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct stdcolt_ext_rt_Block {
    pub ptr: *mut c_void,
    pub size: uint64_t,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct stdcolt_ext_rt_RecipeAllocator {
    pub allocator_sizeof: uint32_t,
    pub allocator_alignof: uint32_t,

    pub allocator_construct: Option<unsafe extern "C" fn(state: *mut c_void) -> int32_t>,
    pub allocator_destruct: Option<unsafe extern "C" fn(state: *mut c_void)>,

    pub allocator_alloc: Option<
        unsafe extern "C" fn(
            state: *mut c_void,
            size: uint64_t,
            align: uint64_t,
        ) -> stdcolt_ext_rt_Block,
    >,

    pub allocator_dealloc:
        Option<unsafe extern "C" fn(state: *mut c_void, block: *const stdcolt_ext_rt_Block)>,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct stdcolt_ext_rt_Allocator {
    pub state: *mut c_void,

    pub allocator_alloc: Option<
        unsafe extern "C" fn(
            state: *mut c_void,
            size: uint64_t,
            align: uint64_t,
        ) -> stdcolt_ext_rt_Block,
    >,

    pub allocator_dealloc:
        Option<unsafe extern "C" fn(state: *mut c_void, block: *const stdcolt_ext_rt_Block)>,

    pub allocator_destruct: Option<unsafe extern "C" fn(state: *mut c_void)>,
}

extern "C" {
    pub fn stdcolt_ext_rt_default_allocator() -> stdcolt_ext_rt_RecipeAllocator;
}
