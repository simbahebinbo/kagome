# https://docs.hunter.sh/en/latest/packages/

# https://docs.hunter.sh/en/latest/packages/pkg/GTest.html
hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

# https://docs.hunter.sh/en/latest/packages/pkg/Boost.html
hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)
