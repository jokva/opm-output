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

#include "config.h"

#if HAVE_DYNAMIC_BOOST_TEST
#define BOOST_TEST_DYN_LINK
#endif

#define BOOST_TEST_MODULE Wells
#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <stdexcept>
#include <fstream>

#include <ert/ecl/ecl_kw_magic.h>
#include <ert/ecl/ecl_sum.h>
#include <ert/ecl/ecl_file.h>
#include <ert/util/util.h>
#include <ert/util/TestArea.hpp>

#include <opm/output/data/Wells.hpp>
#include <opm/output/data/Cells.hpp>
#include <opm/output/eclipse/Tables.hpp>

#include <opm/parser/eclipse/EclipseState.hpp>
#include <opm/parser/eclipse/Parser.hpp>

using namespace Opm;

struct setup {
    EclipseState es;
    ERT::TestArea ta;

    setup( const std::string& name, const std::string& path , const ParseContext& parseContext = ParseContext( )) :
        es( ecl::parse( path, parseContext ) ),
        ta( ERT::TestArea("test_tables" + name ) )
    {
    }

};




BOOST_AUTO_TEST_CASE(Test_PVTX) {
    setup cfg( "PVTO" , "table_deck.DATA");
    Tables tables( cfg.es.getUnits() );

    tables.addPVTO( cfg.es.getTableManager().getPvtoTables() );
    tables.addPVTG( cfg.es.getTableManager().getPvtgTables() );
    {
        ERT::FortIO f("TEST.INIT" , std::fstream::out);
        tables.fwrite( f );
    }


    {
        ecl_file_type * f = ecl_file_open("TEST.INIT" , 0 );
        const ecl_kw_type * tabdims = ecl_file_iget_named_kw( f , "TABDIMS" , 0 );
        const ecl_kw_type * tab = ecl_file_iget_named_kw( f , "TAB" , 0 );

        /* PVTO */
        {
            int offset = ecl_kw_iget_int( tabdims , TABDIMS_IBPVTO_OFFSET_ITEM );
            int rs_offset = ecl_kw_iget_int( tabdims , TABDIMS_JBPVTO_OFFSET_ITEM );
            int column_stride = ecl_kw_iget_int( tabdims , TABDIMS_NRPVTO_ITEM ) * ecl_kw_iget_int( tabdims , TABDIMS_NPPVTO_ITEM ) * ecl_kw_iget_int( tabdims , TABDIMS_NTPVTO_ITEM );

            BOOST_CHECK_EQUAL( 2, ecl_kw_iget_int( tabdims , TABDIMS_NRPVTO_ITEM ) );
            BOOST_CHECK_EQUAL( 5, ecl_kw_iget_int( tabdims , TABDIMS_NPPVTO_ITEM ) );
            BOOST_CHECK_EQUAL( 1, ecl_kw_iget_int( tabdims , TABDIMS_NTPVTO_ITEM ) );

            BOOST_CHECK_CLOSE(50.0            , ecl_kw_iget_double( tab , offset ), 1e-6 );
            BOOST_CHECK_CLOSE(1.0 / 1.10615   , ecl_kw_iget_double( tab , offset + column_stride), 1e-6 );
            BOOST_CHECK_CLOSE(1.18 / 1.10615  , ecl_kw_iget_double( tab , offset + 2*column_stride ), 1e-6 );

            BOOST_CHECK_CLOSE(150.0            , ecl_kw_iget_double( tab , 4 + offset ), 1e-6 );
            BOOST_CHECK_CLOSE(1.0 / 1.08984    , ecl_kw_iget_double( tab , 4 + offset + column_stride), 1e-6 );
            BOOST_CHECK_CLOSE(1.453 / 1.08984  , ecl_kw_iget_double( tab , 4 + offset + 2*column_stride ), 1e-6 );

            BOOST_CHECK_CLOSE(20.59            , ecl_kw_iget_double( tab , rs_offset ), 1e-6 );
            BOOST_CHECK_CLOSE(28.19            , ecl_kw_iget_double( tab , rs_offset + 1), 1e-6 );
        }


        /* PVTG */
        {
            int offset = ecl_kw_iget_int( tabdims , TABDIMS_IBPVTG_OFFSET_ITEM );
            int pg_offset = ecl_kw_iget_int( tabdims , TABDIMS_JBPVTG_OFFSET_ITEM );
            int column_stride = ecl_kw_iget_int( tabdims , TABDIMS_NRPVTG_ITEM ) * ecl_kw_iget_int( tabdims , TABDIMS_NPPVTG_ITEM ) * ecl_kw_iget_int( tabdims , TABDIMS_NTPVTG_ITEM );

            BOOST_CHECK_EQUAL( 2, ecl_kw_iget_int( tabdims , TABDIMS_NRPVTG_ITEM ) );
            BOOST_CHECK_EQUAL( 3, ecl_kw_iget_int( tabdims , TABDIMS_NPPVTG_ITEM ) );
            BOOST_CHECK_EQUAL( 1, ecl_kw_iget_int( tabdims , TABDIMS_NTPVTG_ITEM ) );

            BOOST_CHECK_CLOSE(0.00002448  , ecl_kw_iget_double( tab , offset ), 1e-6 );
            BOOST_CHECK_CLOSE(0.061895    , ecl_kw_iget_double( tab , offset + column_stride), 1e-6 );
            BOOST_CHECK_CLOSE(0.01299     , ecl_kw_iget_double( tab , offset + 2*column_stride ), 1e-6 );

            BOOST_CHECK_CLOSE(20.0        , ecl_kw_iget_double( tab , pg_offset ), 1e-6 );
            BOOST_CHECK_CLOSE(40.0        , ecl_kw_iget_double( tab , pg_offset + 1), 1e-6 );
        }


        /* PVTW */
        /*
          This test code should be OK; however it turns out the PVTW
          tabular data is not really internalized in the EclipseState
          object so it is commented out for now.

        {
            int offset = ecl_kw_iget_int( tabdims , TABDIMS_IBPVTW_OFFSET_ITEM );
            int column_stride = ecl_kw_iget_int( tabdims , TABDIMS_NTPVTW_ITEM );
            BOOST_CHECK_CLOSE( 247.7 , ecl_kw_iget_double( tab , offset ));
            BOOST_CHECK_CLOSE( 1.0 / 1.03665 , ecl_kw_iget_double( tab , offset + column_stride));
            BOOST_CHECK_CLOSE( 0.41726E-04 , ecl_kw_iget_double( tab , offset + 2 * column_stride));
            BOOST_CHECK_CLOSE( 1.03665 / 0.29120 , ecl_kw_iget_double( tab , offset + 3 * column_stride));

            // For the last column - WATER_VISCOSIBILITY there is
            // clearly a transform involved; not really clear which
            // transform this is. This column is therefor not tested.
        }
        */
        ecl_file_close( f );
    }
}
