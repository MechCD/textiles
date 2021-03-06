#include <iostream>

#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/point_cloud.h>
#include <pcl/console/parse.h>
#include <pcl/common/transforms.h>
#include <pcl/visualization/pcl_visualizer.h>
//-- Normals
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/filter.h>
//-- Contours
#include <pcl/filters/extract_indices.h>
//-- Octree
#include <pcl/octree/octree.h>
//-- Projection
#include <pcl/ModelCoefficients.h>
#include <pcl/filters/project_inliers.h>
//-- Bounding box
#include <pcl/features/moment_of_inertia_estimation.h>
#include <vector>
#include <pcl/visualization/cloud_viewer.h>
//-- RSD estimation
#include <pcl/features/normal_3d.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/rsd.h>

//-- My classes
#include "MeshPreprocessor.hpp"
#include "HistogramImageCreator.hpp"

#include <fstream>

//-- Select which parts of the algorithm to enable
#define CURVATURE
#define HISTOGRAM

void show_usage(char * program_name)
{
  std::cout << std::endl;
  std::cout << "Usage: " << program_name << " cloud_filename.[pcd|ply]" << std::endl;
  std::cout << "-h:  Show this help." << std::endl;
  std::cout << "-t, --threshold:  Distance threshold for RANSAC (default: 0.03)" << std::endl;
#ifdef HISTOGRAM
  std::cout << "--histogram: Output file for histogram image" << std::endl;
#endif
#ifdef CURVATURE
  std::cout << "-r, --rsd: Output file for RSD data" << std::endl;
  std::cout << "--rsd-params: Parameters for RSD (kdtree radius for normals, for curvature and plane threshold" << std::endl;
#endif
}

int main(int argc, char* argv[])
{
    //-- Main program parameters (default values)
    double threshold = 0.03;
    std::string output_histogram_image = "histogram_image.m";
    std::string output_rsd_data = "rsd_data.m";
    double rsd_normal_radius = 0.05;
    double rsd_curvature_radius = 0.07;
    double rsd_plane_threshold = 0.2;

    //-- Show usage
    if (pcl::console::find_switch(argc, argv, "-h") || pcl::console::find_switch(argc, argv, "--help"))
    {
        show_usage(argv[0]);
        return 0;
    }

    //-- Check for parameters in arguments
    if (pcl::console::find_switch(argc, argv, "-t"))
        pcl::console::parse_argument(argc, argv, "-t", threshold);
    else if (pcl::console::find_switch(argc, argv, "--threshold"))
        pcl::console::parse_argument(argc, argv, "--threshold", threshold);

    if (pcl::console::find_switch(argc, argv, "--histogram"))
        pcl::console::parse_argument(argc, argv, "--histogram", output_histogram_image);

    if (pcl::console::find_switch(argc, argv, "-r"))
        pcl::console::parse_argument(argc, argv, "-r", output_rsd_data);
    else if (pcl::console::find_switch(argc, argv, "--rsd"))
        pcl::console::parse_argument(argc, argv, "--rsd", output_rsd_data);

    if (pcl::console::find_switch(argc, argv, "--rsd-params"))
        pcl::console::parse_3x_arguments(argc, argv, "--rsd-params", rsd_normal_radius, rsd_curvature_radius,
                                         rsd_plane_threshold);

    //-- Get point cloud file from arguments
    std::vector<int> filenames;
    bool file_is_pcd = false;

    filenames = pcl::console::parse_file_extension_argument(argc, argv, ".ply");

    if (filenames.size() != 1)
    {
        filenames = pcl::console::parse_file_extension_argument(argc, argv, ".pcd");

        if (filenames.size() != 1)
        {
            std::cerr << "No input file specified!" << std::endl;
            show_usage(argv[0]);
            return -1;
        }

        file_is_pcd = true;
    }

    //-- Print arguments to user
    std::cout << "Selected arguments: " << std::endl
              << "\tRANSAC threshold: " << threshold << std::endl
#ifdef HISTOGRAM
              << "\tHistogram image: " << output_histogram_image << std::endl
#endif
#ifdef CURVATURE
              << "\tRSD:" << std::endl
              << "\t\tFile: " << output_rsd_data << std::endl
              << "\t\tNormal search radius: " << rsd_normal_radius << std::endl
              << "\t\tCurvature search radius: " << rsd_curvature_radius << std::endl
              << "\t\tPlane threshold: " << rsd_plane_threshold << std::endl
#endif
              << "\tInput file: " << argv[filenames[0]] << std::endl;



    //-- Load point cloud data
    pcl::PointCloud<pcl::PointXYZ>::Ptr source_cloud(new pcl::PointCloud<pcl::PointXYZ>);

    if (file_is_pcd)
    {
        if (pcl::io::loadPCDFile(argv[filenames[0]], *source_cloud) < 0)
        {
            std::cout << "Error loading point cloud " << argv[filenames[0]] << std::endl << std::endl;
            show_usage(argv[0]);
            return -1;
        }
    }
    else
    {
        if (pcl::io::loadPLYFile(argv[filenames[0]], *source_cloud) < 0)
        {
            std::cout << "Error loading point cloud " << argv[filenames[0]] << std::endl << std::endl;
            show_usage(argv[0]);
            return -1;
        }
    }

    /********************************************************************************************
    * Stuff goes on here
    *********************************************************************************************/
    //-- Initial pre-processing of the mesh
    //------------------------------------------------------------------------------------
    std::cout << "[+] Pre-processing the mesh..." << std::endl;
    pcl::PointCloud<pcl::PointXYZ>::Ptr garment_points(new pcl::PointCloud<pcl::PointXYZ>);
    MeshPreprocessor<pcl::PointXYZ> preprocessor;
    preprocessor.setRANSACThresholdDistance(threshold);
    preprocessor.setInputCloud(source_cloud);
    preprocessor.process(*garment_points);

    //-- Find bounding box (not really required)
    pcl::MomentOfInertiaEstimation<pcl::PointXYZ> feature_extractor;
    pcl::PointXYZ min_point_AABB, max_point_AABB;
    pcl::PointXYZ min_point_OBB,  max_point_OBB;
    pcl::PointXYZ position_OBB;
    Eigen::Matrix3f rotational_matrix_OBB;

    feature_extractor.setInputCloud(garment_points);
    feature_extractor.compute();
    feature_extractor.getAABB(min_point_AABB, max_point_AABB);
    feature_extractor.getOBB(min_point_OBB, max_point_OBB, position_OBB, rotational_matrix_OBB);

#ifdef CURVATURE
    //-- Curvature stuff
    //-----------------------------------------------------------------------------------
    std::cout << "[+] Calculating curvature descriptors..." << std::endl;
    // Object for storing the normals.
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
    // Object for storing the RSD descriptors for each point.
    pcl::PointCloud<pcl::PrincipalRadiiRSD>::Ptr descriptors(new pcl::PointCloud<pcl::PrincipalRadiiRSD>());


    // Note: you would usually perform downsampling now. It has been omitted here
    // for simplicity, but be aware that computation can take a long time.

    // Estimate the normals.
    pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::Normal> normalEstimation;
    normalEstimation.setInputCloud(garment_points);
    normalEstimation.setRadiusSearch(rsd_normal_radius);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr kdtree(new pcl::search::KdTree<pcl::PointXYZ>);
    normalEstimation.setSearchMethod(kdtree);
    normalEstimation.setViewPoint(0, 0 , 2);
    normalEstimation.compute(*normals);

    // RSD estimation object.
    pcl::RSDEstimation<pcl::PointXYZ, pcl::Normal, pcl::PrincipalRadiiRSD> rsd;
    rsd.setInputCloud(garment_points);
    rsd.setInputNormals(normals);
    rsd.setSearchMethod(kdtree);
    // Search radius, to look for neighbors. Note: the value given here has to be
    // larger than the radius used to estimate the normals.
    rsd.setRadiusSearch(rsd_curvature_radius);
    // Plane radius. Any radius larger than this is considered infinite (a plane).
    rsd.setPlaneRadius(rsd_plane_threshold);
    // Do we want to save the full distance-angle histograms?
    rsd.setSaveHistograms(false);

    rsd.compute(*descriptors);

    //-- Save to mat file
    std::ofstream rsd_file(output_rsd_data.c_str());
    for (int i = 0; i < garment_points->points.size(); i++)
    {
        rsd_file << garment_points->points[i].x << " "
                 << garment_points->points[i].y << " "
                 << garment_points->points[i].z << " "
                 << descriptors->points[i].r_min << " "
                 << descriptors->points[i].r_max << "\n";
    }
    rsd_file.close();

#endif

#ifdef HISTOGRAM
    //-- Obtain histogram image
    //-----------------------------------------------------------------------------------
    std::cout << "[+] Calculating histogram image..." << std::endl;
    HistogramImageCreator<pcl::PointXYZ> histogram_image_creator;
    histogram_image_creator.setInputPointCloud(garment_points);
    histogram_image_creator.setResolution(1024);
    histogram_image_creator.setUpsampling(true);
    histogram_image_creator.compute();
    Eigen::MatrixXi image = histogram_image_creator.getDepthImageAsMatrix();

    //-- Temporal fix to get image (through file)
    std::ofstream file(output_histogram_image.c_str());
    file << image;
    file.close();
#endif

    return 0;
}
