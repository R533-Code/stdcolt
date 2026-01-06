#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

use core::ffi::c_void;

pub type uint64_t = u64;
pub type uint32_t = u32;
pub type int32_t = i32;

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct stdcolt_ext_rt_Key {
    pub key: *const c_void,
    pub size: uint64_t,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct stdcolt_ext_rt_RecipePerfectHashFunction {
    pub phf_sizeof: uint32_t,
    pub phf_alignof: uint32_t,

    pub phf_construct: Option<
        unsafe extern "C" fn(
            state: *mut c_void,
            keys: *const stdcolt_ext_rt_Key,
            keys_len: uint64_t,
        ) -> int32_t,
    >,

    pub phf_destruct: Option<unsafe extern "C" fn(state: *mut c_void)>,

    pub phf_lookup: Option<
        unsafe extern "C" fn(state: *mut c_void, key: *const stdcolt_ext_rt_Key) -> uint64_t,
    >,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct stdcolt_ext_rt_PerfectHashFunction {
    pub state: *mut c_void,

    pub phf_lookup: Option<
        unsafe extern "C" fn(state: *mut c_void, key: *const stdcolt_ext_rt_Key) -> uint64_t,
    >,

    pub phf_destruct: Option<unsafe extern "C" fn(state: *mut c_void)>,
}

extern "C" {
    pub fn stdcolt_ext_rt_default_perfect_hash_function() -> stdcolt_ext_rt_RecipePerfectHashFunction;
}
