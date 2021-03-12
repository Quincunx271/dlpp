#include <dl++.hpp>

#include <catch2/catch.hpp>

using Catch::Matchers::Contains;
using Catch::Matchers::StartsWith;
using Catch::Matchers::VectorContains;

TEST_CASE("Can load and use shared object")
{
    dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);

    auto getCounter = lib.sym<int()>("getCounter");
    auto incCounter = lib.sym<void()>("incCounter");
    CHECK(getCounter() == 0);
    incCounter();
    CHECK(getCounter() == 1);
}

TEST_CASE("Can load and use  multiple shared objects")
{
    dlpp::dl lib1(dlpp::lmid_t::newlm(), "libcounter.so", dlpp::dl_flags::now);
    dlpp::dl lib2(dlpp::lmid_t::newlm(), "libcounter.so", dlpp::dl_flags::now);

    auto getCounter1 = lib1.sym<int()>("getCounter");
    auto incCounter1 = lib1.sym<void()>("incCounter");
    auto getCounter2 = lib2.sym<int()>("getCounter");
    auto incCounter2 = lib2.sym<void()>("incCounter");

    CHECK(getCounter1() == 0);
    incCounter1();
    CHECK(getCounter1() == 1);
    incCounter1();

    CHECK(getCounter2() == 0);
    incCounter2();
    CHECK(getCounter2() == 1);
    incCounter2();

    CHECK(getCounter1() == 2);
    incCounter1();
    CHECK(getCounter1() == 3);
    incCounter1();

    CHECK(getCounter2() == 2);
    incCounter2();
    CHECK(getCounter2() == 3);
    incCounter2();
}

TEST_CASE("Loading nonexistent shared library has nice error")
{
    CHECK_THROWS_AS(dlpp::dl("libnonexistantlibrarythatisverylongandcannotexistfordlpptestcase.so",
                        dlpp::dl_flags::now),
        dlpp::dl_error);

    CHECK_THROWS_WITH(
        dlpp::dl("libnonexistantlibrarythatisverylongandcannotexistfordlpptestcase.so",
            dlpp::dl_flags::now),
        Contains("libnonexistantlibrarythatisverylongandcannotexistfordlpptestcase.so"));
}

TEST_CASE("Loading nonexistent symbol has nice error")
{
    dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);

    CHECK_THROWS_AS(lib.sym<int()>("getCounter2"), dlpp::dl_error);
    CHECK_THROWS_WITH(
        lib.sym<int()>("getCounter2"), Contains("libcounter.so") && Contains("getCounter2"));
}

TEST_CASE("info_lmid works")
{
    SECTION("base")
    {
        dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);
        dlpp::lmid_t lmid = lib.info_lmid();
        CHECK(lmid == dlpp::lmid_t::base());
    }
    SECTION("newlm")
    {
        dlpp::dl lib(dlpp::lmid_t::newlm(), "libcounter.so", dlpp::dl_flags::now);
        dlpp::lmid_t lmid = lib.info_lmid();
        CHECK(lmid != dlpp::lmid_t::base());
    }
}

TEST_CASE("info_linkmap works")
{
    dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);
    dlpp::link_map map = lib.info_linkmap();
    REQUIRE(map);
    CHECK_THAT(std::string(map.name()), Contains("tools/exampleso/libcounter.so"));
}

TEST_CASE("info_origin works")
{
    dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);
    CHECK_THAT(lib.info_origin(), Contains("tools/exampleso") && StartsWith("/"));
}

TEST_CASE("info_serinfo works")
{
    dlpp::dl lib("libcounter.so", dlpp::dl_flags::now);
    dlpp::serinfo ser = lib.info_serinfo();
    std::vector<std::string_view> paths;
    for (unsigned int i = 0; i < ser.cnt(); ++i) {
        paths.push_back(ser.serpath(i).name());
    }

    CHECK_THAT(paths,
        VectorContains(std::string_view("tools/exampleso"))
            && VectorContains(std::string_view("/lib")));
}
