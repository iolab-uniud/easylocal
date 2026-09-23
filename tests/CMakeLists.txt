add_executable(
    easylocal_toolchain_smoke
    toolchain_smoke.cpp
)
target_link_libraries(
    easylocal_toolchain_smoke
    PRIVATE
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.toolchain
    COMMAND easylocal_toolchain_smoke
)

add_executable(
    easylocal_header_self_containment
    header_self_containment.cpp
)
target_link_libraries(
    easylocal_header_self_containment
    PRIVATE
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.header-self-containment
    COMMAND easylocal_header_self_containment
)

add_executable(
    easylocal_multi_tu
    odr_main.cpp
    odr_a.cpp
    odr_b.cpp
)
target_link_libraries(
    easylocal_multi_tu
    PRIVATE
        EasyLocal::EasyLocal
)
add_test(
    NAME easylocal.multi-tu
    COMMAND easylocal_multi_tu
)

add_executable(
    easylocal_assignment_model_test
    assignment_model.cpp
)
target_link_libraries(
    easylocal_assignment_model_test
    PRIVATE
        easylocal_assignment_mwe_model
)
add_test(
    NAME easylocal.assignment-model
    COMMAND easylocal_assignment_model_test
)
