function(_cargo_metadata out manifest)
    get_property(
        RUSTC_EXECUTABLE
        TARGET Rust::Rustc PROPERTY IMPORTED_LOCATION
    )
    get_property(
        CARGO_EXECUTABLE
        TARGET Rust::Cargo PROPERTY IMPORTED_LOCATION
    )
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env
                CARGO_BUILD_RUSTC=${RUSTC_EXECUTABLE}
                ${CARGO_EXECUTABLE}
                    metadata
                        --manifest-path ${manifest}
                        --format-version 1
        OUTPUT_VARIABLE json
        COMMAND_ERROR_IS_FATAL ANY
    )

    set(${out} ${json} PARENT_SCOPE)
endfunction()

# Add targets (crates) of one package
function(_generator_add_package_targets workspace_manifest_path package_manifest_path package_name targets profile out_created_targets)
    # target types
    set(has_staticlib FALSE)
    set(has_cdylib FALSE)
    set(corrosion_targets "")


    # Add a custom target with the package (crate) name, as a convenience to build everything in a
    # crate.
    # Note: may cause problems if package_name == bin_name...
    #add_custom_target("${package_name}")
    # todo: verify on windows if this actually needs to be done...
    string(REPLACE "\\" "/" manifest_path "${package_manifest_path}")

    string(JSON targets_len LENGTH "${targets}")
    math(EXPR targets_len-1 "${targets_len} - 1")

    foreach(ix RANGE ${targets_len-1})
        #
        string(JSON target GET "${targets}" ${ix})
        string(JSON target_name GET "${target}" "name")
        string(JSON target_kind GET "${target}" "kind")
        string(JSON target_kind_len LENGTH "${target_kind}")
        string(JSON target_name GET "${target}" "name")

        math(EXPR target_kind_len-1 "${target_kind_len} - 1")
        set(kinds)
        foreach(ix RANGE ${target_kind_len-1})
            string(JSON kind GET "${target_kind}" ${ix})
            list(APPEND kinds ${kind})
        endforeach()

        if(TARGET "${target_name}"
            AND ("staticlib" IN_LIST kinds OR "cdylib" IN_LIST kinds OR "bin" IN_LIST kinds)
            )
            message(WARNING "Failed to import Rust crate ${target_name} (kind: `${target_kind}`) because a target "
                "with the same name already exists. Skipping this target.\n"
                "Help: If you are importing a package which exposes both a `lib` and "
                "a `bin` target, please consider explicitly naming the targets in your `Cargo.toml` manifest.\n"
                "Note: If you have multiple different packages which have targets with the same name, please note that "
                "this is currently not supported by Corrosion. Feel free to open an issue on Github to request "
                "supporting this scenario."
                )
            # Skip this target to prevent a hard error.
            continue()
        endif()

        if("staticlib" IN_LIST kinds OR "cdylib" IN_LIST kinds)
            if("staticlib" IN_LIST kinds)
                set(has_staticlib TRUE)
            endif()

            if("cdylib" IN_LIST kinds)
                set(has_cdylib TRUE)
            endif()
            set(archive_byproducts "")
            set(shared_lib_byproduct "")
            set(pdb_byproduct "")

            _corrosion_add_library_target("${workspace_manifest_path}" "${target_name}" "${has_staticlib}" "${has_cdylib}"
                archive_byproducts shared_lib_byproduct pdb_byproduct)

            set(byproducts "")
            list(APPEND byproducts "${archive_byproducts}" "${shared_lib_byproduct}" "${pdb_byproduct}")

            set(cargo_build_out_dir "")
            _add_cargo_build(
                cargo_build_out_dir
                PACKAGE ${package_name}
                TARGET ${target_name}
                MANIFEST_PATH "${manifest_path}"
                PROFILE "${profile}"
                TARGET_KIND "lib"
                BYPRODUCTS "${byproducts}"
            )
            if(archive_byproducts)
                _corrosion_copy_byproducts(
                    ${target_name} ARCHIVE_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${archive_byproducts}"
                )
            endif()
            if(shared_lib_byproduct)
                _corrosion_copy_byproducts(
                    ${target_name} LIBRARY_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${shared_lib_byproduct}"
                )
            endif()
            if(pdb_byproduct)
                _corrosion_copy_byproducts(
                    ${target_name} PDB_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${pdb_byproduct}"
                )
            endif()
            list(APPEND corrosion_targets ${target_name})

        elseif("bin" IN_LIST kinds)
            set(pdb_byproduct "")
            _corrosion_add_bin_target("${workspace_manifest_path}" "${target_name}"
                "bin_byproduct" "pdb_byproduct"
            )

            set(byproducts "")
            list(APPEND byproducts "${bin_byproduct}" "${pdb_byproduct}")

            set(cargo_build_out_dir "")
            _add_cargo_build(
                cargo_build_out_dir
                PACKAGE "${package_name}"
                TARGET "${target_name}"
                MANIFEST_PATH "${manifest_path}"
                PROFILE "${profile}"
                TARGET_KIND "bin"
                BYPRODUCTS "${byproducts}"
            )
            _corrosion_copy_byproducts(
                    ${target_name} RUNTIME_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${bin_byproduct}"
            )
            if(pdb_byproduct)
                _corrosion_copy_byproducts(
                        ${target_name} PDB_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${pdb_byproduct}"
                )
            endif()
            list(APPEND corrosion_targets ${target_name})
        else()
            # ignore other kinds (like examples, tests, build scripts, ...)
        endif()
    endforeach()

    if(NOT corrosion_targets)
        message(DEBUG "No relevant targets found in package ${package_name} - Ignoring")
    else()
        set_target_properties(${corrosion_targets} PROPERTIES INTERFACE_COR_PACKAGE_MANIFEST_PATH "${package_manifest_path}")
    endif()
    set(${out_created_targets} "${corrosion_targets}" PARENT_SCOPE)

endfunction()


function(_generator_add_cargo_targets)
    set(options "")
    set(one_value_args MANIFEST_PATH PROFILE)
    set(multi_value_args CRATES)
    cmake_parse_arguments(
        GGC
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    _cargo_metadata(json ${GGC_MANIFEST_PATH})
    string(JSON packages GET "${json}" "packages")
    string(JSON workspace_members GET "${json}" "workspace_members")

    string(JSON pkgs_len LENGTH "${packages}")
    math(EXPR pkgs_len-1 "${pkgs_len} - 1")

    string(JSON ws_mems_len LENGTH ${workspace_members})
    math(EXPR ws_mems_len-1 "${ws_mems_len} - 1")

    set(created_targets "")
    foreach(ix RANGE ${pkgs_len-1})
        string(JSON pkg GET "${packages}" ${ix})
        string(JSON pkg_id GET "${pkg}" "id")
        string(JSON pkg_name GET "${pkg}" "name")
        string(JSON pkg_manifest_path GET "${pkg}" "manifest_path")
        string(JSON targets GET "${pkg}" "targets")

        string(JSON targets_len LENGTH "${targets}")
        math(EXPR targets_len-1 "${targets_len} - 1")
        foreach(ix RANGE ${ws_mems_len-1})
            string(JSON ws_mem GET "${workspace_members}" ${ix})
            if(ws_mem STREQUAL pkg_id AND ((NOT GGC_CRATES) OR (pkg_name IN_LIST GGC_CRATES)))
                message(DEBUG "Found ${targets_len} targets in package ${pkg_name}")

                _generator_add_package_targets("${GGC_MANIFEST_PATH}" "${pkg_manifest_path}" "${pkg_name}" "${targets}" "${GGC_PROFILE}" curr_created_targets)
                list(APPEND created_targets "${curr_created_targets}")
            endif()
        endforeach()
    endforeach()

    if(NOT created_targets)
        message(FATAL_ERROR "found no targets in ${pkgs_len} packages")
    else()
        message(DEBUG "Corrosion created the following CMake targets: ${curr_created_targets}")
    endif()
endfunction()
