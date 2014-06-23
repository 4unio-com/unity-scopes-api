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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <unity/scopes/internal/SettingsDB.h>

#include <unity/UnityExceptions.h>
#include <unity/util/FileIO.h>

#include <boost/regex.hpp>  // Use Boost implementation until http://gcc.gnu.org/bugzilla/show_bug.cgi?id=53631 is fixed.
#include <gtest/gtest.h>

using namespace unity;
using namespace unity::scopes::internal;
using namespace std;

string const db_name = TEST_BIN_DIR "/db";
string const add_cmd = TEST_SRC_DIR "/add_doc.py";

void add_doc(const string& doc_id, const string& json)
{
    ostringstream s;
    s << add_cmd << " " << db_name << " " << doc_id << " '" << json << "'";
    system(s.str().c_str());
}

TEST(SettingsDB, basic)
{
    auto schema = unity::util::read_text_file(TEST_SRC_DIR "/schema.json");

    {
        unlink(db_name.c_str());
        SettingsDB db(db_name, schema);

        // If db doesn't exist, default values are returned.
        auto s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("London", s["locationSetting"].get_string());
        EXPECT_EQ(1, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Create the DB now, but without any recognizable records.
        add_doc("xyz", R"( { "a": "b" } )");

        // Default values must still be there.
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("London", s["locationSetting"].get_string());
        EXPECT_EQ(1, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Add a record that looks like a settings document, but for which there is no schema.
        add_doc("default-xyz", R"( { "value": "b" } )");

        // Default values must still be there.
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("London", s["locationSetting"].get_string());
        EXPECT_EQ(1, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Change the location.
        add_doc("default-locationSetting", R"( { "value": "New York" } )");
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("New York", s["locationSetting"].get_string());
        EXPECT_EQ(1, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Change the unit.
        add_doc("default-unitTempSetting", R"( { "value": 0 } )");
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("New York", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Change the age.
        add_doc("default-ageSetting", R"( { "value": 42 } )");
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("New York", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(42, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Call settings again. This causes state_changed_ in the implementation
        // to remain false because the generation count hasn't changed, meaning
        // that we must see the same unchanged values as last time.
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("New York", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(42, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Change enabled boolean.
        add_doc("default-enabledSetting", R"( { "value": false } )");
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("New York", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(42, s["ageSetting"].get_int());
        EXPECT_FALSE(s["enabledSetting"].get_bool());
    }

    {
        unlink(db_name.c_str());
        auto schema = unity::util::read_text_file(TEST_SRC_DIR "/schema.json");
        SettingsDB db(db_name, schema);

        // Add a record with a typo, so the value won't be found.
        add_doc("default-locationSetting", R"( { "VaLuE": "New York" } )");

        // Check that we still get the default values.
        auto s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("London", s["locationSetting"].get_string());
        EXPECT_EQ(1, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());
    }

    {
        unlink(db_name.c_str());

        // This schema does not specify a default value for location and unit.
        auto schema = unity::util::read_text_file(TEST_SRC_DIR "/no_default_schema.json");
        SettingsDB db(db_name, schema);

        // Check that we now can see only the other two values. (DB doesn't exist yet.)
        auto s = db.settings();
        EXPECT_EQ(2, s.size());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Add records to supply the values.
        add_doc("default-locationSetting", R"( { "value": "Paris" } )");
        add_doc("default-unitTempSetting", R"( { "value": 0 } )");

        // Check that they are correct
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("Paris", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());
    }

    {
        unlink(db_name.c_str());

        // This schema does not specify a default value for location and unit.
        auto schema = unity::util::read_text_file(TEST_SRC_DIR "/no_default_schema.json");
        SettingsDB db(db_name, schema);

        // Add records to supply the values.
        add_doc("default-locationSetting", R"( { "value": "Paris" } )");
        add_doc("default-unitTempSetting", R"( { "value": 0 } )");

        // Check that they are correct.
        auto s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("Paris", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());

        // Update a value with a nonsense setting, to make sure that nonsense settings are ignored.
        add_doc("default-ageSetting", R"( { "value": [ "Paris", "New York" ] } )");

        // Check that nothing has changed.
        s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("Paris", s["locationSetting"].get_string());
        EXPECT_EQ(0, s["unitTempSetting"].get_int());
        EXPECT_EQ(23, s["ageSetting"].get_int());
        EXPECT_TRUE(s["enabledSetting"].get_bool());
    }
}

TEST(SettingsDB, exceptions)
{
    try
    {
        SettingsDB db("unused", "syntax error");
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_STREQ("unity::ResourceException: SettingsDB(): cannot parse schema, db = unused:\n"
                     "    unity::ResourceException: SettingsSchema(): cannot parse schema: * Line 1, Column 1\n"
                     "  Syntax error: value, object or array expected.\n",
                     e.what());
    }

    try
    {
        // Open DB that doesn't exist yet.
        unlink(db_name.c_str());
        auto schema = unity::util::read_text_file(TEST_SRC_DIR "/schema.json");
        SettingsDB db(db_name, schema);

        // Add a record, which creates the DB
        add_doc("default-locationSetting", R"( { "value": "New York" } )");

        // Remove read permission.
        system(string("chmod -r " + db_name).c_str());

        // Call settings(), which will try to open the DB and fail.
        db.settings();
        FAIL();
    }
    catch (ResourceException const& e)
    {
        EXPECT_EQ("unity::ResourceException: SettingsDB: u1db_open() failed, db = " + db_name, e.what());
    }

    try
    {
        unlink(db_name.c_str());

        // Create DB with one record.
        add_doc("default-locationSetting", R"( { "value": "Munich" } )");

        auto schema = unity::util::read_text_file(TEST_SRC_DIR "/schema.json");
        SettingsDB db(db_name, schema);

        auto s = db.settings();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ("Munich", s["locationSetting"].get_string());

        // Clobber the DB.
        system((string("cat >") + db_name + " <<EOF\nx\nEOF" + db_name).c_str());

        // Call settings(), which will fail.
        db.settings();
        FAIL();
    }
    catch (ResourceException const& e)
    {
        boost::regex r(string("unity::ResourceException: SettingsDB\\(\\): u1db_whats_changed\\(\\) failed, "
                              "status = [0-9]+, db = ") + db_name);
        EXPECT_TRUE(boost::regex_match(e.what(), r)) << e.what();
    }
}
