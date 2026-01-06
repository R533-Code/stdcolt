# `runtime_type`:
A performant extension of `stdcolt` for runtime type binding and creations.

It is designed with specific goals in mind:
- acting as a common runtime "language" for communicating across different programming languages,
- type erasing member/method accesses for ABI stability,
- runtime reflection.

The main API is in `C`, but the actual implementation is in `C++`.