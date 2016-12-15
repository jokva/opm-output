/*
  Copyright 2014 Andreas Lauser

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

#define BOOST_TEST_MODULE EclipseWriter
#include <boost/test/unit_test.hpp>

#include <opm/output/eclipse/EclipseWriter.hpp>
#include <opm/output/data/Cells.hpp>

#include <opm/parser/eclipse/Parser.hpp>
#include <opm/parser/eclipse/EclipseState.hpp>
#include <opm/parser/eclipse/Units.hpp>

// ERT stuff
#include <ert/ecl/ecl_kw.h>
#include <ert/ecl/ecl_endian_flip.h>
#include <ert/ecl/ecl_file.h>
#include <ert/ecl/ecl_util.h>
#include <ert/util/ert_unique_ptr.hpp>
#include <ert/util/TestArea.hpp>

#include <memory>

using namespace Opm;

data::Solution createBlackoilState( int timeStepIdx, int numCells ) {

    std::vector< double > pressure( numCells );
    std::vector< double > swat( numCells );
    std::vector< double > sgas( numCells );
    std::vector< double > rs( numCells );
    std::vector< double > rv( numCells );

    for( int cellIdx = 0; cellIdx < numCells; ++cellIdx) {

        pressure[cellIdx] = timeStepIdx*1e5 + 1e4 + cellIdx;
        sgas[cellIdx] = timeStepIdx*1e5 +2.2e4 + cellIdx;
        swat[cellIdx] = timeStepIdx*1e5 +2.3e4 + cellIdx;

        // oil vaporization factor
        rv[cellIdx] = timeStepIdx*1e5 +3e4 + cellIdx;
        // gas dissolution factor
        rs[cellIdx] = timeStepIdx*1e5 + 4e4 + cellIdx;
    }

    data::Solution solution;

    solution.insert( "PRESSURE" , UnitSystem::measure::pressure , pressure, data::TargetType::RESTART_SOLUTION );
    solution.insert( "SWAT" , UnitSystem::measure::identity , swat, data::TargetType::RESTART_SOLUTION );
    solution.insert( "SGAS" , UnitSystem::measure::identity , sgas, data::TargetType::RESTART_SOLUTION );
    solution.insert( "RS" , UnitSystem::measure::identity , rs, data::TargetType::RESTART_SOLUTION );
    solution.insert( "RV" , UnitSystem::measure::identity , rv, data::TargetType::RESTART_SOLUTION );

    return solution;
}

template< typename T >
std::vector< T > getErtData( ecl_kw_type *eclKeyword ) {
    size_t kwSize = ecl_kw_get_size(eclKeyword);
    T* ertData = static_cast< T* >(ecl_kw_iget_ptr(eclKeyword, 0));

    return { ertData, ertData + kwSize };
}

template< typename T, typename U >
void compareErtData(const std::vector< T > &src,
                    const std::vector< U > &dst,
                    double tolerance ) {
    BOOST_CHECK_EQUAL(src.size(), dst.size());
    if (src.size() != dst.size())
        return;

    for (size_t i = 0; i < src.size(); ++i)
        BOOST_CHECK_CLOSE(src[i], dst[i], tolerance);
}

void compareErtData(const std::vector<int> &src, const std::vector<int> &dst)
{
    BOOST_CHECK_EQUAL_COLLECTIONS( src.begin(), src.end(),
                                   dst.begin(), dst.end() );
}

void checkEgridFile( const EclipseGrid& eclGrid ) {
    // use ERT directly to inspect the EGRID file produced by EclipseWriter
    auto egridFile = fortio_open_reader("FOO.EGRID", /*isFormated=*/0, ECL_ENDIAN_FLIP);

    const auto numCells = eclGrid.getNX() * eclGrid.getNY() * eclGrid.getNZ();

    while( auto* eclKeyword = ecl_kw_fread_alloc( egridFile ) ) {
        std::string keywordName(ecl_kw_get_header(eclKeyword));
        if (keywordName == "COORD") {
            std::vector< double > sourceData;
            eclGrid.exportCOORD( sourceData );
            auto resultData = getErtData< float >( eclKeyword );
            compareErtData(sourceData, resultData, 1e-6);
        }
        else if (keywordName == "ZCORN") {
            std::vector< double > sourceData;
            eclGrid.exportZCORN(sourceData);
            auto resultData = getErtData< float >( eclKeyword );
            compareErtData(sourceData, resultData, /*percentTolerance=*/1e-6);
        }
        else if (keywordName == "ACTNUM") {
            std::vector< int > sourceData( numCells );
            eclGrid.exportACTNUM(sourceData);
            auto resultData = getErtData< int >( eclKeyword );

            if( sourceData.empty() )
                sourceData.assign( numCells, 1 );

            compareErtData( sourceData, resultData );
        }

        ecl_kw_free(eclKeyword);
    }

    fortio_fclose(egridFile);
}

void checkInitFile( const EclipseState& es, const data::Solution& simProps) {
    // use ERT directly to inspect the INIT file produced by EclipseWriter
    ERT::ert_unique_ptr<ecl_file_type , ecl_file_close> initFile(ecl_file_open( "FOO.INIT" , 0 ));

    for (int k=0; k < ecl_file_get_size(  initFile.get() ); k++) {
        ecl_kw_type * eclKeyword = ecl_file_iget_kw( initFile.get( ) , k );
        std::string keywordName(ecl_kw_get_header(eclKeyword));

        if (keywordName == "PORO") {
            const auto& sourceData = es.get3DProperties()
                                       .getDoubleGridProperty( "PORO" )
                                       .getData();
            auto resultData = getErtData< float >( eclKeyword );
            compareErtData(sourceData, resultData, 1e-4);
        }

        if (keywordName == "PERMX") {
            const auto& sourceData = es.get3DProperties()
                                       .getDoubleGridProperty( "PERMX" )
                                       .getData();
            auto resultData = getErtData< float >( eclKeyword );

            // convert the data from ERT from Field to SI units (mD to m^2)
            for (size_t i = 0; i < resultData.size(); ++i) {
                resultData[i] *= 9.869233e-16;
            }

            compareErtData(sourceData, resultData, 1e-4);
        }
    }

    BOOST_CHECK( ecl_file_has_kw( initFile.get() , "FIPNUM" ));
    BOOST_CHECK( ecl_file_has_kw( initFile.get() , "SATNUM" ));

    for (const auto& prop : simProps) {
        BOOST_CHECK( ecl_file_has_kw( initFile.get() , prop.first.c_str()) );
    }
}

void checkRestartFile( int timeStepIdx ) {
    for (int i = 1; i <= timeStepIdx; ++i) {
        auto sol = createBlackoilState( i, 3 * 3 * 3 );

        // use ERT directly to inspect the restart file produced by EclipseWriter
        auto rstFile = fortio_open_reader("FOO.UNRST", /*isFormated=*/0, ECL_ENDIAN_FLIP);

        int curSeqnum = -1;
        while( auto* eclKeyword = ecl_kw_fread_alloc(rstFile) ) {
            std::string keywordName(ecl_kw_get_header(eclKeyword));

            if (keywordName == "SEQNUM") {
                curSeqnum = *static_cast<int*>(ecl_kw_iget_ptr(eclKeyword, 0));
            }
            if (curSeqnum != i)
                continue;

            if (keywordName == "PRESSURE") {
                const auto resultData = getErtData< float >( eclKeyword );
                for( auto& x : sol.data("PRESSURE") )
                    x /= Metric::Pressure;

                compareErtData( sol.data("PRESSURE"), resultData, 1e-4 );
            }

            if (keywordName == "SWAT") {
                const auto resultData = getErtData< float >( eclKeyword );
                compareErtData(sol.data("SWAT"), resultData, 1e-4);
            }

            if (keywordName == "SGAS") {
                const auto resultData = getErtData< float >( eclKeyword );
                compareErtData( sol.data("SGAS"), resultData, 1e-4 );
            }

            if (keywordName == "KRO")
                BOOST_CHECK_EQUAL( 1.0 * i * ecl_kw_get_size( eclKeyword ) , ecl_kw_element_sum_float( eclKeyword ));

            if (keywordName == "KRG")
                BOOST_CHECK_EQUAL( 10.0 * i * ecl_kw_get_size( eclKeyword ) , ecl_kw_element_sum_float( eclKeyword ));
        }

        fortio_fclose(rstFile);
    }
}

BOOST_AUTO_TEST_CASE(EclipseWriterIntegration) {
    const char *deckString =
        "RUNSPEC\n"
        "UNIFOUT\n"
        "OIL\n"
        "GAS\n"
        "WATER\n"
        "METRIC\n"
        "DIMENS\n"
        "3 3 3/\n"
        "GRID\n"
        "INIT\n"
        "DXV\n"
        "1.0 2.0 3.0 /\n"
        "DYV\n"
        "4.0 5.0 6.0 /\n"
        "DZV\n"
        "7.0 8.0 9.0 /\n"
        "TOPS\n"
        "9*100 /\n"
        "PROPS\n"
        "PORO\n"
        "27*0.3 /\n"
        "PERMX\n"
        "27*1 /\n"
        "REGIONS\n"
        "SATNUM\n"
        "27*2 /\n"
        "FIPNUM\n"
        "27*3 /\n"
        "SOLUTION\n"
        "RPTRST\n"
        "BASIC=2\n"
        "/\n"
        "SCHEDULE\n"
        "TSTEP\n"
        "1.0 2.0 3.0 4.0 5.0 6.0 7.0 /\n"
        "WELSPECS\n"
        "'INJ' 'G' 1 1 2000 'GAS' /\n"
        "'PROD' 'G' 3 3 1000 'OIL' /\n"
        "/\n";

    ERT::TestArea ta("test_ecl_writer");

    auto write_and_check = [&]( int first = 1, int last = 5 ) {
        auto es = ecl::parseString( deckString );
        auto& eclGrid = es.getInputGrid();
        es.getIOConfig().setBaseName( "FOO" );

        EclipseWriter eclWriter( es, eclGrid );

        using measure = UnitSystem::measure;
        using TargetType = data::TargetType;
        auto start_time = ecl_util_make_date( 10, 10, 2008 );
        std::vector<double> tranx(3*3*3);
        std::vector<double> trany(3*3*3);
        std::vector<double> tranz(3*3*3);
        data::Solution eGridProps {
            { "TRANX", { measure::transmissibility, tranx, TargetType::INIT } },
            { "TRANY", { measure::transmissibility, trany, TargetType::INIT } },
            { "TRANZ", { measure::transmissibility, tranz, TargetType::INIT } },
        };


        eclWriter.writeInitial( );
        eclWriter.writeInitial( eGridProps );

        data::Wells wells;

        for( int i = first; i < last; ++i ) {
            data::Solution sol = createBlackoilState( i, 3 * 3 * 3 );
            sol.insert("KRO", measure::identity , std::vector<double>(3*3*3 , i), TargetType::RESTART_AUXILLARY);
            sol.insert("KRG", measure::identity , std::vector<double>(3*3*3 , i*10), TargetType::RESTART_AUXILLARY);


            auto first_step = ecl_util_make_date( 10 + i, 11, 2008 );
            eclWriter.writeTimeStep( i,
                    false,
                    first_step - start_time,
                    sol,
                    wells);


            checkRestartFile( i );
        }

        checkInitFile( es, eGridProps);
        checkEgridFile( eclGrid );

        std::ifstream file( "FOO.UNRST", std::ios::binary );
        std::streampos file_size = 0;

        file_size = file.tellg();
        file.seekg( 0, std::ios::end );
        file_size = file.tellg() - file_size;

        return file_size;
    };

    /*
     * write the file and calculate the file size. FOO.UNRST should be
     * overwitten for every time step, i.e. the file size should not change
     * between runs.  This is to verify that UNRST files are properly
     * overwritten, which they used not to.
     *
     * * https://github.com/OPM/opm-simulators/issues/753
     * * https://github.com/OPM/opm-output/pull/61
     */
    const auto file_size = write_and_check();

    for( int i = 0; i < 3; ++i )
        BOOST_CHECK_EQUAL( file_size, write_and_check() );

    /*
     * check that "restarting" and writing over previous timesteps does not
     * change the file size, if the total amount of steps are the same
     */
    BOOST_CHECK_EQUAL( file_size, write_and_check( 3, 5 ) );

    /* verify that adding steps from restart also increases file size */
    BOOST_CHECK( file_size < write_and_check( 3, 7 ) );

    /*
     * verify that restarting a simulation, then writing fewer steps truncates
     * the file
     */
    BOOST_CHECK_EQUAL( file_size, write_and_check( 3, 5 ) );
}

BOOST_AUTO_TEST_CASE(OPM_XWEL) {
}
