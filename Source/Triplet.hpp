/*
 This file is part of SpatGRIS.

 Developers: Samuel Béland, Olivier Bélanger, Nicolas Masson

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "OutputPatch.hpp"

//==============================================================================
struct Triplet {
    output_patch_t id1{};
    output_patch_t id2{};
    output_patch_t id3{};
    //==============================================================================
    [[nodiscard]] constexpr bool contains(output_patch_t const outputPatch) const noexcept;
    [[nodiscard]] constexpr bool isSameAs(Triplet const & other) const noexcept;
};

//==============================================================================
constexpr bool Triplet::contains(output_patch_t const outputPatch) const noexcept
{
    return id1 == outputPatch || id2 == outputPatch || id3 == outputPatch;
}

//==============================================================================
constexpr bool Triplet::isSameAs(Triplet const & other) const noexcept
{
    return contains(other.id1) && contains(other.id2) && contains(other.id3);
}
