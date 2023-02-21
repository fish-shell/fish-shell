use std::error::Error;
use std::path::PathBuf;
use std::rc::Rc;

#[derive(Clone)]
pub enum CargoTargetType {
    Executable,
    Library {
        has_staticlib: bool,
        has_cdylib: bool,
    },
}

#[derive(Clone)]
pub struct CargoTarget {
    cargo_package: Rc<cargo_metadata::Package>,
    cargo_target: cargo_metadata::Target,
    target_type: CargoTargetType,
    workspace_manifest_path: Rc<PathBuf>,
}

impl CargoTarget {
    pub fn from_metadata(
        cargo_package: Rc<cargo_metadata::Package>,
        cargo_target: cargo_metadata::Target,
        workspace_manifest_path: Rc<PathBuf>,
    ) -> Option<Self> {
        let target_type = if cargo_target
            .kind
            .iter()
            .any(|k| k == "staticlib" || k == "cdylib")
        {
            CargoTargetType::Library {
                has_staticlib: cargo_target.kind.iter().any(|k| k == "staticlib"),
                has_cdylib: cargo_target.kind.iter().any(|k| k == "cdylib"),
            }
        } else if cargo_target.kind.iter().any(|k| k == "bin") {
            CargoTargetType::Executable
        } else {
            return None;
        };

        Some(Self {
            cargo_package,
            cargo_target,
            target_type,
            workspace_manifest_path,
        })
    }

    pub fn emit_cmake_target(
        &self,
        out_file: &mut dyn std::io::Write,
        cargo_profile: Option<&str>,
    ) -> Result<(), Box<dyn Error>> {

        let cargo_build_profile_option = if let Some(profile) = cargo_profile {
            format!("PROFILE \"{}\"", profile)
        } else {
            String::default()
        };

        writeln!(out_file, "set(byproducts \"\")
                            set(cargo_build_out_dir \"\")
                            set(archive_byproducts \"\")
                            set(shared_lib_byproduct \"\")
                            set(pdb_byproduct \"\")
                            set(bin_byproduct \"\")
        ")?;

        let target_kind = match self.target_type {
            CargoTargetType::Library {
                has_staticlib,
                has_cdylib,
            } => {
                assert!(has_staticlib || has_cdylib);
                let ws_manifest = self
                    .workspace_manifest_path
                    .to_str()
                    .expect("Non-utf8 path encountered")
                    .replace("\\", "/");

                writeln!(
                    out_file,
                    "
                    _corrosion_add_library_target(\"{workspace_manifest_path}\"
                            \"{target_name}\"
                            \"{has_staticlib}\"
                            \"{has_cdylib}\"
                            archive_byproducts
                            shared_lib_byproduct
                            pdb_byproduct
                    )
                    list(APPEND byproducts
                            \"${{archive_byproducts}}\"
                            \"${{shared_lib_byproduct}}\"
                            \"${{pdb_byproduct}}\"
                    )
                    ",
                    // todo: check if this should be the workspace manifest (probably yes)
                    workspace_manifest_path = ws_manifest,
                    target_name = self.cargo_target.name,
                    has_staticlib = has_staticlib,
                    has_cdylib = has_cdylib,
                )?;
                "lib"
            }
            CargoTargetType::Executable => {
                let ws_manifest = self
                    .workspace_manifest_path
                    .to_str()
                    .expect("Non-utf8 path encountered")
                    .replace("\\", "/");
                writeln!(
                    out_file,
                    "
                    _corrosion_add_bin_target(\"{workspace_manifest_path}\" \"{target_name}\"
                        bin_byproduct pdb_byproduct
                    )
                    set(byproducts \"\")
                    list(APPEND byproducts \"${{bin_byproduct}}\" \"${{pdb_byproduct}}\")
                    ",
                    workspace_manifest_path = ws_manifest,
                    target_name = self.cargo_target.name,
                )?;
                "bin"
            }
        };
        writeln!(out_file,
            "
            set(cargo_build_out_dir \"\")
            _add_cargo_build(
                cargo_build_out_dir
                PACKAGE \"{package_name}\"
                TARGET \"{target_name}\"
                MANIFEST_PATH \"{package_manifest_path}\"
                {profile_option}
                TARGET_KIND \"{target_kind}\"
                BYPRODUCTS \"${{byproducts}}\"
            )

            set_target_properties({target_name} PROPERTIES
                INTERFACE_COR_PACKAGE_MANIFEST_PATH \"{package_manifest_path}\"
            )

            if(archive_byproducts)
                _corrosion_copy_byproducts(
                    {target_name} ARCHIVE_OUTPUT_DIRECTORY \"${{cargo_build_out_dir}}\" \"${{archive_byproducts}}\"
                )
            endif()
            if(shared_lib_byproduct)
                _corrosion_copy_byproducts(
                    {target_name} LIBRARY_OUTPUT_DIRECTORY \"${{cargo_build_out_dir}}\" \"${{shared_lib_byproduct}}\"
                )
            endif()
            if(pdb_byproduct)
                _corrosion_copy_byproducts(
                    {target_name} PDB_OUTPUT_DIRECTORY \"${{cargo_build_out_dir}}\" \"${{pdb_byproduct}}\"
                )
            endif()
            if(bin_byproduct)
                _corrosion_copy_byproducts(
                    {target_name} RUNTIME_OUTPUT_DIRECTORY \"${{cargo_build_out_dir}}\" \"${{bin_byproduct}}\"
                )
            endif()
            ",
            package_name = self.cargo_package.name,
            target_name = self.cargo_target.name,
            package_manifest_path = self.cargo_package.manifest_path.as_str().replace("\\", "/"),
            profile_option = cargo_build_profile_option,
            target_kind = target_kind,

        )?;
        Ok(())
    }

}
