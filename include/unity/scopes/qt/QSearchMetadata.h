/*
 * Copyright (C) 2015 Canonical Ltd
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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#pragma once

#ifndef _ENABLE_QT_EXPERIMENTAL_
#error You should define _ENABLE_QT_EXPERIMENTAL_ in order to use this experimental header file.
#endif

#include <unity/util/DefinesPtrs.h>
#include <unity/scopes/Location.h>

#include <QtCore/QString>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#include <QtCore/QVariantMap>
#pragma GCC diagnostic pop

namespace unity
{

namespace scopes
{

namespace qt
{

namespace internal
{
class QSearchMetadataImpl;
}

/**
\brief Metadata passed with search requests.
*/
class QSearchMetadata final
{
public:
    /// @cond
    UNITY_DEFINES_PTRS(QSearchMetadata);
    /// @endcond

    /**
    \brief Create SearchMetadata with the given locale and form factor.
    \param locale locale string, eg. en_EN
    \param form_factor form factor name, e.g. phone, desktop, phone-version etc.
    */
    QSearchMetadata(QString const& locale, QString const& form_factor);

    /**
    \brief Create SearchMetadata with the given cardinality, locale, and form factor.
    \param cardinality maximum number of search results
    \param locale locale string, eg. en_EN
    \param form_factor form factor name, e.g. phone, desktop, phone-version etc.
    */
    QSearchMetadata(int cardinality, QString const& locale, QString const& form_factor);

    /**@name Copy and assignment
    Copy and assignment operators (move and non-move versions) have the usual value semantics.
    */
    //{@
    QSearchMetadata(QSearchMetadata const& other);
    QSearchMetadata(QSearchMetadata&&);

    QSearchMetadata& operator=(QSearchMetadata const& other);
    QSearchMetadata& operator=(QSearchMetadata&&);
    //@}

    /// @cond
    ~QSearchMetadata();
    /// @endcond

    /**
    \brief Set cardinality.
    \param cardinality The maximum number of search results.
    */
    void set_cardinality(int cardinality);

    /**
    \brief Get cardinality.
    \return The maxmium number of search results, or 0 for no limit.
    */
    int cardinality() const;

    /**
    \brief Set location.
    \param location Location data.
    */
    void set_location(Location const& location);

    /**
    \brief Get location.
    \return Location data representing the current location, including attributes such as city and country.
    \throws unity::NotFoundException if no location data is available.
    */
    Location location() const;

    /**
    \brief Does the SearchMetadata have a location.
    \return True if there is a location property.
    */
    bool has_location() const;

    /**
    \brief Remove location data entirely.

    This method does nothing if no location data is present.
    */
    void remove_location();

    /**
    \brief Sets a hint.

    \param key The name of the hint.
    \param value Hint value
    */
    void set_hint(QString const& key, QVariant const& value);

    /**
    \brief Get all hints.

    \return Hints dictionary.
    \throws unity::NotFoundException if no hints are available.
    */
    QVariantMap hints() const;

    /**
    \brief Check if this SearchMetadata has a hint.
    \param key The hint name.
    \return True if the hint is set.
    */
    bool contains_hint(QString const& key) const;

    /**
    \brief Returns a reference to a hint.

    This method can be used to read or set hints. Setting a value of an existing hint overwrites
    its previous value.
    Referencing a non-existing hint automatically creates it with a default value of Variant::Type::Null.
    \param key The name of the hint.
    \return A reference to the hint.
    */
    QVariant& operator[](QString const& key);

    /**
    \brief Returns a const reference to a hint.

    This method can be used for read-only access to hints.
    Referencing a non-existing hint throws unity::InvalidArgumentException.
    \param key The name of the hint.
    \return A const reference to the hint.
    \throws unity::NotFoundException if no hint with the given name exists.
    */
    QVariant const& operator[](QString const& key) const;

private:
    /// @cond
    std::unique_ptr<internal::QSearchMetadataImpl> p;
    /// @endcond
};

}  // namespace qt

}  // namespace scopes

}  // namespace unity
