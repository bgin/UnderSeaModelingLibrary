######################################################################
# USML studies

    set( USML_STUDIES_DIR ${PROJECT_SOURCE_DIR}/studies
         CACHE PATH "directory for data used in testing" )
    
    add_executable( cmp_speed studies/cmp_speed/cmp_speed.cc )
    target_link_libraries( cmp_speed usml )
    
    add_executable( ray_speed studies/ray_speed/ray_speed.cc )
    target_link_libraries( ray_speed usml )
    
    add_executable( eigenray_extra_test studies/eigenray_extra/eigenray_extra_test.cc )
    target_link_libraries( eigenray_extra_test usml )
    
    add_executable( pedersen_test studies/pedersen/pedersen_test.cc )
    target_link_libraries( pedersen_test usml )
    
    add_executable( malta_movie studies/malta_movie/malta_movie.cc )
    target_link_libraries( malta_movie usml )
    
    add_executable( malta_rays studies/malta_movie/malta_rays.cc )
    target_link_libraries( malta_rays usml )
    
    add_executable( waveq3d_visual studies/waveq3d_visual/waveq3d_visual.cc )
    target_link_libraries( waveq3d_visual usml )
    
    set_property(
       TARGET cmp_speed ray_speed eigenray_extra_test pedersen_test malta_movie malta_rays waveq3d_visual
       PROPERTY COMPILE_DEFINITIONS
        USML_DATA_DIR="${USML_DATA_DIR}"
        USML_STUDIES_DIR="${USML_STUDIES_DIR}"
       )

