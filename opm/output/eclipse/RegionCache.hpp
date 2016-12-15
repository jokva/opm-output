/*
  Copyright 2016 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPM_REGION_CACHE_HPP
#define OPM_REGION_CACHE_HPP

#include <map>
#include <vector>

namespace Opm {
    class EclipseState;
    class EclipseGrid;

namespace out {
    class RegionCache {
    public:
        RegionCache() = default;
        RegionCache(const EclipseState& state, const EclipseGrid& grid);
        const std::vector<size_t>& cells( int region_id ) const;
        const std::vector<std::pair<std::string,size_t>>& completions( int region_id ) const;

    private:
        std::vector<size_t> cells_empty;
        std::vector<std::pair<std::string,size_t>> completions_empty;

        std::map<int , std::vector<size_t> > cell_map;
        std::map<int , std::vector<std::pair<std::string,size_t>>> completion_map;
    };
}
}

#endif
