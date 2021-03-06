#include <iostream>
#include <vector>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/point_types.h>
#include <pcl/console/parse.h>

#include "Debug.hpp"

int main(int argc, char* argv[])
{
    //-- Load data
    //-----------------------------------------------------------------------------------------------
    //-- Show usage
    if (pcl::console::find_switch(argc, argv, "-h") || pcl::console::find_switch(argc, argv, "--help"))
    {
        std::cout << "This program requires the user to specify an input file (pcd/ply)" << std::endl;
        return 0;
    }

    //-- Get point cloud file from arguments
    std::vector<int> filenames;
    bool file_is_pcd = false;

    filenames = pcl::console::parse_file_extension_argument(argc, argv, ".ply");

    if (filenames.size() != 1)
    {
        filenames = pcl::console::parse_file_extension_argument(argc, argv, ".pcd");

        if (filenames.size() != 1)
        {
            std::cout << "This program requires the user to specify an input file (pcd/ply)" << std::endl;
            return -1;
        }
        else
        {
            file_is_pcd = true;
        }
    }

    //-- Load point cloud data
    pcl::PointCloud<pcl::PointXYZ>::Ptr source_cloud(new pcl::PointCloud<pcl::PointXYZ>);

    if (file_is_pcd)
    {
        if (pcl::io::loadPCDFile(argv[filenames[0]], *source_cloud) < 0)
        {
            std::cout << "Error loading point cloud " << argv[filenames[0]] << std::endl << std::endl;
            std::cout << "This program requires the user to specify an input file (pcd/ply)" << std::endl;
            return -1;
        }
    }
    else
    {
        if (pcl::io::loadPLYFile(argv[filenames[0]], *source_cloud) < 0)
        {
            std::cout << "Error loading point cloud " << argv[filenames[0]] << std::endl << std::endl;
            std::cout << "This program requires the user to specify an input file (pcd/ply)" << std::endl;
            return -1;
        }
    }

    //-- Test begins here
    //-----------------------------------------------------------------------------------------------
    Debug debug;
    debug.setEnabled(false);
    debug.plotPointCloud<pcl::PointXYZ>(source_cloud, Debug::COLOR_RED);
    debug.show("test1");
    std::cout << "Nothing should happen here, output is disabled." << std::endl;
    std::cout << "Press key to continue>" << std::endl;
    std::cin.get();

    debug.setEnabled(true);
    debug.plotPointCloud<pcl::PointXYZ>(source_cloud, Debug::COLOR_BLUE);
    debug.show("test2");
    std::cout << "Now output is enabled, a viewer should show up. Only blue stuff is plotted." << std::endl;
    std::cout << "Close display window to continue." << std::endl;

    debug.plotPointCloud<pcl::PointXYZ>(source_cloud, Debug::COLOR_YELLOW);
    debug.show("test3");
    std::cout << "Another cloud is printed, a SINGLE viewer should show up. Only yellow stuff is plotted." << std::endl;
    std::cout << "Close display window to continue."<< std::endl;

    pcl::ModelCoefficients coeffs;
    coeffs.values.push_back(0.0);
    coeffs.values.push_back(0.0);
    coeffs.values.push_back(1.0);
    coeffs.values.push_back(0.0);
    debug.plotPlane(coeffs, Debug::COLOR_GREEN);
    debug.show("Plane1");
    std::cout << "A nice green plane should show up on a new window." << std::endl;
    std::cout << "Close display window to continue." << std::endl;;

    debug.plotPlane(0, 1, 0, 0, Debug::COLOR_MAGENTA);
    debug.show("Plane2");
    std::cout << "A nice magenta plane should show up on a new window." << std::endl;
    std::cout << "Close display window to continue." << std::endl;;
}
