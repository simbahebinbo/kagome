#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

add_library(runtime_wavm
		compartment_wrapper.cpp
    intrinsics/intrinsic_functions.cpp
    intrinsics/intrinsic_module.cpp
    intrinsics/intrinsic_module_instance.cpp
    intrinsics/intrinsic_resolver_impl.cpp
    instance_environment_factory.cpp
    module.cpp
    module_params.cpp
    module_factory_impl.cpp
    module_instance.cpp
		memory_impl.cpp
    wavm_external_memory_provider.cpp
    wavm_internal_memory_provider.cpp
    )
target_link_libraries(runtime_wavm
		runtime_common
		${LLVM_LIBS}
		WAVM::libWAVM
		core_api_factory
        executor
		Boost::boost
        trie_storage_provider
		memory_allocator
    )
kagome_install(runtime_wavm)
