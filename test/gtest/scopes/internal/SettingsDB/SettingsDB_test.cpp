/*
 * Copyright (C) 2014 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Michi Henning <michi.henning@canonical.com>
 *   Pete Woods <pete.woods@canonical.com>
 */

#define BOOST_NO_CXX11_SCOPED_ENUMS // We need this to successfully link against Boost when calling
                                    // copy_file. See https://svn.boost.org/trac/boost/ticket/6779
#include <boost/filesystem/operations.hpp>

#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <unity/scopes/internal/SettingsDB.h>

#include <unity/UnityExceptions.h>

#include <boost/regex.hpp>  // Use Boost implementation until http://gcc.gnu.org/bugzilla/show_bug.cgi?id=53631 is fixed.
#include <gtest/gtest.h>
#include <fcntl.h>
#include <ctime>
#include <thread>

using namespace unity;
using namespace unity::scopes::internal;
using namespace std;

string const db_name = TEST_BIN_DIR "/foo.ini";

void write_db(const string& src)
{
    int fd = ::open(db_name.c_str(), O_WRONLY|O_CREAT);
    if (fd == -1)
    {
        FAIL() << "Couldn't open file " << db_name << " " << errno;
    }

    struct flock fl;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_pid = getpid();

    if (::fcntl(fd, F_SETLKW, &fl) != 0)
    {
        FAIL() << "Couldn't get write lock for " << db_name << " " << errno;
    }

    string pth = string(TEST_SRC_DIR) + "/" + src;
    int fd2 = ::open(pth.c_str(), O_RDONLY);
    if (fd2 == -1)
    {
        FAIL() << "Failed to open input file " << pth;
    }

    // copy the file
    int n = 0;
    char buf[1024];
    while ((n = read(fd2, buf, 1024)) > 0)
    {
        write(fd, buf, n);
    }
    close (fd2);

    close(fd);

    // make sure the next write doesn't happen too fast or otherwise modification time of settings db
    // will be the same and change will not be detected by SettingsDB. Note, this is not an issue with
    // real use cases when settings are modified by the UI (and SettingsDB uses nanosecond-based mtime).
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

#define TRY_EXPECT_EQ(expected, actual) \
{ \
  auto now = chrono::system_clock::now(); \
  auto later = now + chrono::seconds(10); \
  while (now < later) \
  { \
    if ((expected) == (actual)) \
    { \
        break; \
    } \
    this_thread::sleep_for(chrono::milliseconds(10)); \
    now = chrono::system_clock::now(); \
  } \
  EXPECT_EQ((expected), (actual)); \
}

#define TRY_EXPECT_TRUE(expr) \
{ \
  auto now = chrono::system_clock::now(); \
  auto later = now + chrono::seconds(10); \
  while (now < later) \
  { \
    if (expr) \
    { \
        break; \
    } \
    this_thread::sleep_for(chrono::milliseconds(10)); \
    now = chrono::system_clock::now(); \
  } \
  EXPECT_TRUE(expr); \
}

#define TRY_EXPECT_FALSE(expr) \
{ \
  TRY_EXPECT_TRUE(!(expr)); \
}

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(test_logger, boost::log::sources::severity_channel_logger_mt<>)

TEST(SettingsDB, basic)
{
    auto schema = TEST_SRC_DIR "/schema.ini";

    {
        unlink(db_name.c_str());
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        // If db doesn't exist, default values are returned.
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("London", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Create the DB now, but without any recognizable records.
        write_db("db_empty.ini");

        // Default values must still be there.
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("London", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Add a record that looks like a settings document, but for which there is no schema.
        write_db("db_unknown_setting.ini");

        // Default values must still be there.
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("London", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Change the location.
        write_db("db_location.ini");
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Change the unit.
        write_db("db_loctemp.ini");
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        TRY_EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Change the age.
        write_db("db_loctempage.ini");
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        TRY_EXPECT_EQ(42.0, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Call settings again. This causes state_changed_ in the implementation
        // to remain false because the generation count hasn't changed, meaning
        // that we must see the same unchanged values as last time.
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(42, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Change enabled boolean.
        write_db("db_loctempageenabled.ini");
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(42, db->settings()["ageSetting"].get_double());
        TRY_EXPECT_FALSE(db->settings()["enabledSetting"].get_bool());
    }

    {
        unlink(db_name.c_str());

        // This schema does not specify a default value for location and unit.
        auto schema = TEST_SRC_DIR "/no_default_schema.ini";
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        // Check that we now can see only the other two values. (DB doesn't exist yet.)
        EXPECT_EQ(2, db->settings().size());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Add records to supply the values.
        write_db("db_loctemp.ini");

        // Check that they are correct
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());
    }

    {
        unlink(db_name.c_str());

        // This schema does not specify a default value for location and unit.
        auto schema = TEST_SRC_DIR "/no_default_schema.ini";
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        // Add records to supply the values.
        write_db("db_loctemp.ini");

        // Check that they are correct.
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Update a value with a nonsense setting, to make sure that nonsense settings are ignored.
        write_db("db_loctemp_bad_age.ini");

        // Check that nothing has changed.
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("Paris", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());
    }
}

TEST(SettingsDB, chinese_characters)
{
    auto schema = TEST_SRC_DIR "/chinese_schema.ini";

    {
        unlink(db_name.c_str());
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        // If db doesn't exist, default values are returned.
        EXPECT_EQ(1, db->settings().size());
        EXPECT_EQ("的", db->settings()["locationSetting"].get_string());

        // Change the location.
        write_db("db_chinese_location.ini");
        EXPECT_EQ(1, db->settings().size());
        TRY_EXPECT_EQ("丈", db->settings()["locationSetting"].get_string());
    }
}

TEST(SettingsDB, delete_db)
{
    auto schema = TEST_SRC_DIR "/schema.ini";

    {
        unlink(db_name.c_str());
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        // If db doesn't exist, default values are returned.
        EXPECT_EQ(4, db->settings().size());
        EXPECT_EQ("London", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());

        // Create the DB now, with a bunch of settings.
        write_db("db_loctempageenabled.ini");

        // Settings should be updated.
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("New York", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(0, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(42, db->settings()["ageSetting"].get_double());
        EXPECT_FALSE(db->settings()["enabledSetting"].get_bool());

        // Now remove the database.
        unlink(db_name.c_str());

        // Default values should come back.
        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("London", db->settings()["locationSetting"].get_string());
        EXPECT_EQ(1, db->settings()["unitTempSetting"].get_int());
        EXPECT_EQ(23, db->settings()["ageSetting"].get_double());
        EXPECT_TRUE(db->settings()["enabledSetting"].get_bool());
    }
}

TEST(SettingsDB, from_json_string)
{
    char const* ok_schema = R"delimiter(
    {
        "settings":
        [
            {
                "id": "location",
                "displayName": "Location",
                "type": "string",
                "parameters": {
                    "defaultValue": "London"
                }
            }
        ]
    }
    )delimiter";

    {
        unlink(db_name.c_str());
        auto db = SettingsDB::create_from_json_string(db_name, ok_schema, test_logger::get());

        // If db doesn't exist, default values are returned.
        EXPECT_EQ(1, db->settings().size());
        EXPECT_EQ("London", db->settings()["location"].get_string());

        // Change the location.
        write_db("db_location_json.ini");
        EXPECT_EQ(1, db->settings().size());
        TRY_EXPECT_EQ("New York", db->settings()["location"].get_string());
    }
}

TEST(SettingsDB, exceptions)
{
    try
    {
        auto db = SettingsDB::create_from_ini_file("unused", "no_such_file", test_logger::get());
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_STREQ("unity::ResourceException: SettingsDB::create_from_ini_file(): schema = no_such_file, db = unused:\n"
                     "    unity::ResourceException: IniSettingsSchema(): cannot parse settings file \"no_such_file\":\n"
                     "        unity::FileException: Could not load ini file no_such_file: No such file or directory (errno = 4)",
                     e.what());
    }

    try
    {
        auto db = SettingsDB::create_from_json_string("unused", "syntax error", test_logger::get());
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_STREQ("unity::ResourceException: SettingsDB::create_from_json_string(): cannot parse schema, db = unused:\n"
                     "    unity::ResourceException: JsonSettingsSchema(): cannot parse schema: * Line 1, Column 1\n"
                     "  Syntax error: value, object or array expected.\n",
                     e.what());
    }

    try
    {
        unlink(db_name.c_str());

        // Create DB with one record.
        write_db("db_location_munich.ini");

        auto schema = TEST_SRC_DIR "/schema.ini";
        auto db = SettingsDB::create_from_ini_file(db_name, schema, test_logger::get());

        EXPECT_EQ(4, db->settings().size());
        TRY_EXPECT_EQ("Munich", db->settings()["locationSetting"].get_string());

        // Clobber the DB.
        if (system((string("cat >") + db_name + " <<EOF\nx\nEOF" + db_name).c_str()) < 0)
        {
            FAIL();
        }
        usleep(100000);

        // Call settings(), which will fail.
        db->settings();
        FAIL();
    }
    catch (ResourceException const& e)
    {
        boost::regex r("unity::ResourceException: unity::FileException: Could not load ini file .*: Key file contains line 'x' which is not a key-value pair, group, or comment \\(errno = 1\\)");
        EXPECT_TRUE(boost::regex_match(e.what(), r)) << e.what();
    }
}
