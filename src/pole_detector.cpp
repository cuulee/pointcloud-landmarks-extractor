#include <pole_detector.h>

// ** Helper functions!!!

bool compareClusterHeight(Cluster& first, Cluster& second){
	return (first.getCentroid()[2] < second.getCentroid()[2]);
}

void makeColoredPoint(pcl::PointXYZRGB & PtColored, pcl::PointXYZ const & Pt, uint32_t rgb){
	PtColored.x = Pt.x;
	PtColored.y = Pt.y;
	PtColored.z = Pt.z;
	PtColored.rgb = *reinterpret_cast<float*>(&rgb);
}

void eigenV4f2PointXYZ(Eigen::Vector4f const &vec, pcl::PointXYZ &point){
	point.x = vec[0];
	point.y = vec[1];
	point.z = vec[2];
}

void PointXYZ2eigenV4f2D(Eigen::Vector4f &vec, pcl::PointXYZ const &point){
	vec[0] = point.x;
	vec[1] = point.y;
	vec[2] = 1; // In order to check only x-y distance
	vec[3] = 0; // Check!!!!!!*************//////////********
}

// ** Class implementation begins!!!

Cluster::Cluster(Eigen::Vector4f centroid_, double radius_, pcl::PointXYZ minPt_, pcl::PointXYZ maxPt_, pcl::PointCloud<pcl::PointXYZ>::Ptr clusterCloud_):
	centroid(centroid_),
	radius(radius_),
	minPt(minPt_),
	maxPt(maxPt_),
	processed(false),
	clusterCloud(clusterCloud_){}

Cluster::Cluster(){
	clusterCloud = pcl::PointCloud<pcl::PointXYZ>::Ptr (new pcl::PointCloud<pcl::PointXYZ>);
}

Segment::Segment(){
	height = 0;
}


void Segment::addCluster(Cluster& cluster){
	segmentParts.push_back(cluster);
	int numClusters = segmentParts.size();
	centroid = (centroid * (numClusters-1) + cluster.getCentroid())/(numClusters);
	if (numClusters == 1){
		minPt = cluster.getMinPt();
		maxPt = cluster.getMaxPt();
		segmentCloud = cluster.getClusterCloud();
	}
	else{
		minPt.x = min(minPt.x, cluster.getMinPt().x);
		maxPt.x = max(maxPt.x, cluster.getMaxPt().x);
		minPt.y = min(minPt.y, cluster.getMinPt().y);
		maxPt.y = max(maxPt.y, cluster.getMaxPt().y);
		minPt.z = min(minPt.z, cluster.getMinPt().z);
		maxPt.z = max(maxPt.z, cluster.getMaxPt().z);
		*segmentCloud += *(cluster.getClusterCloud());
	}
	height = maxPt.z - minPt.z;
}


void Segment::mergeSegment(Segment& segment){
	segmentParts.insert(segmentParts.end(), segment.getSegmentParts().begin(), segment.getSegmentParts().end());
	double lenSelf = segmentParts.size();
	double lenSegment = segment.getSegmentParts().size();
	centroid = (segment.getCentroid() * lenSegment + centroid * lenSelf)/(lenSelf + lenSegment);
	minPt.x = min(minPt.x, segment.getMinPt().x);
	maxPt.x = max(maxPt.x, segment.getMaxPt().x);
	minPt.y = min(minPt.y, segment.getMinPt().y);
	maxPt.y = max(maxPt.y, segment.getMaxPt().y);
	minPt.z = min(minPt.z, segment.getMinPt().z);
	maxPt.z = max(maxPt.z, segment.getMaxPt().z);
	height = maxPt.z - minPt.z;
}


PCLPoleDetector::PCLPoleDetector(){
	inCloud = pcl::PointCloud<pcl::PointXYZ>::Ptr (new pcl::PointCloud<pcl::PointXYZ>);
	processCloud = pcl::PointCloud<pcl::PointXYZ>::Ptr (new pcl::PointCloud<pcl::PointXYZ>);
	clusterCloud = pcl::PointCloud<pcl::PointXYZRGB>::Ptr (new pcl::PointCloud<pcl::PointXYZRGB>);
	stitchedCloud = pcl::PointCloud<pcl::PointXYZRGB>::Ptr (new pcl::PointCloud<pcl::PointXYZRGB>);
	poleCloud = pcl::PointCloud<pcl::PointXYZRGB>::Ptr (new pcl::PointCloud<pcl::PointXYZRGB>);
	_donCloud = pcl::PointCloud<pcl::PointNormal>::Ptr (new pcl::PointCloud<pcl::PointNormal>);
	/* Uncomment to enable visualization
	viewer = boost::shared_ptr<pcl::visualization::PCLVisualizer> (new pcl::visualization::PCLVisualizer ("Pole Detector"));
	viewer->setBackgroundColor (0, 0, 0);
	viewer->addCoordinateSystem (3.0, "coordinates", 0);
	//*/
}


PCLPoleDetector::~PCLPoleDetector(){
	filteredCluster.clear();
	detectedPoles.clear();
	extractedTrees.clear();
}


void PCLPoleDetector::pointCloudVisualizer(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud, string name){
	boost::random::uniform_int_distribution<> dist(0, 255);
	uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
	pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> colourHandler(cloud, r, g, b);
	viewer->addPointCloud<pcl::PointXYZ> (cloud, colourHandler, name);
	viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, name);
}


void PCLPoleDetector::pointCloudVisualizer(list<Cluster> const &clusterList, string id){
	boost::random::uniform_int_distribution<> dist(0, 255);
	uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
	int j = 0;
	pcl::PointXYZ center;
	for (list<Cluster>::const_iterator it = clusterList.begin(); it != clusterList.end(); ++it){
		Cluster cluster = *it;
		eigenV4f2PointXYZ(cluster.getCentroid(), center);
		viewer->addSphere(center, cluster.getRadius()/2, r, g, b, id+boost::lexical_cast<string>(j));
		j++;
	}
}

void PCLPoleDetector::pointCloudVisualizer(Cluster &cluster, string id){
	boost::random::uniform_int_distribution<> dist(0, 255);
	uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
	pcl::PointXYZ center;
	eigenV4f2PointXYZ(cluster.getCentroid(), center);
	viewer->addSphere(center, cluster.getRadius()/2, r, g, b, id);
}


void PCLPoleDetector::readPCD(string pathToFile){
	pcl::PCDReader reader;
	reader.read (pathToFile, *inCloud);
	// pcl::io::loadPCDFile<pcl::PointXYZ>(pathToFile, *inCloud);
	// std::vector<int> indices;
	// pcl::removeNaNFromPointCloud(*inCloud, *inCloud, indices);
	// cerr << "PointCloud size before: " << inCloud->width * inCloud->height << " data points." << endl;
	// Should I make a copy? Is the nmodified input cloud required at any other stage?
	processCloud = inCloud;
	// pointCloudVisualizer(inCloud, 'r', "Input cloud");
}

void PCLPoleDetector::readDON(string pathToFile){
	pcl::PCDReader reader;
	reader.read(pathToFile, *_donCloud);
	cerr << "Size of DON input cloud: " << _donCloud->width * _donCloud->height << " data points." << endl;
}


void PCLPoleDetector::writePCD(string pathToFile){
	pcl::PCDWriter writer;
	/*
	writer.writeBinary("clusters.pcd", *clusterCloud);
	// pcl::io::savePCDFileASCII (pathToFile, *processCloud);
  	std::cerr << "Saved " << clusterCloud->points.size() << " data points to clusters.pcd." << std::endl;

  	writer.writeBinary("poles.pcd", *poleCloud);
	// pcl::io::savePCDFileASCII (pathToFile, *processCloud);
  	std::cerr << "Saved " << poleCloud->points.size() << " data points to poles.pcd." << std::endl;

  	writer.writeBinary("stitches.pcd", *stitchedCloud);
	// pcl::io::savePCDFileASCII (pathToFile, *processCloud);
  	std::cerr << "Saved " << stitchedCloud->points.size() << " data points to stitches.pcd." << std::endl;
	//*/

  	writer.writeBinary(pathToFile, *poleCloud);
	// pcl::io::savePCDFileASCII (pathToFile, *processCloud);
  	std::cerr << "Saved " << poleCloud->points.size() << " data points to " + pathToFile << std::endl;


}

void PCLPoleDetector::writePoles(){
	pcl::PCDWriter writer;
	int i = 0;
	for (std::list<Segment>::iterator it = detectedPoles.begin(); it != detectedPoles.end(); ++it){
		writer.writeBinary("poles/pole" + boost::lexical_cast<std::string>(i) + ".pcd", *(it->getSegmentCloud()));
		i += 1;
	}
}

void PCLPoleDetector::writeTrees(){
	pcl::PCDWriter writer;
	int i = 0;
	for (std::list<Segment>::iterator it = extractedTrees.begin(); it != extractedTrees.end(); ++it){
		writer.writeBinary("trees/tree" + boost::lexical_cast<std::string>(i) + ".pcd", *(it->getSegmentCloud()));
		i += 1;
	}
}


void PCLPoleDetector::statisticalOutlierRemover(double mean, double stdDev){
	// Create the filtering object
	pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor;
	sor.setInputCloud (processCloud);
	sor.setMeanK (mean);
	sor.setStddevMulThresh (stdDev);
	sor.filter (*processCloud);
}



void PCLPoleDetector::groundPlaneRemover(double distThreshold){
	// Create the segmentation object for the planar model and set all the parameters
  	pcl::SACSegmentation<pcl::PointXYZ> seg;
  	pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
  	pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
  	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_plane (new pcl::PointCloud<pcl::PointXYZ> ());
  	seg.setOptimizeCoefficients (true);
  	seg.setModelType (pcl::SACMODEL_PLANE);
  	seg.setMethodType (pcl::SAC_RANSAC);
  	seg.setMaxIterations (100);
  	seg.setDistanceThreshold (distThreshold);

  	int j=0, nr_points = (int) processCloud->points.size ();
  	while (processCloud->points.size () > 0.3 * nr_points)
  	{
    	// Segment the largest planar component from the remaining cloud
    	seg.setInputCloud (processCloud);
    	seg.segment (*inliers, *coefficients);
   		if (inliers->indices.size () == 0)
    	{
      		std::cout << "Could not estimate a planar model for the given dataset." << std::endl;
      		break;
    	}

    	// Extract the planar inliers from the input cloud
    	pcl::ExtractIndices<pcl::PointXYZ> extract;
    	extract.setInputCloud (processCloud);
    	extract.setIndices (inliers);
    	/*
    	extract.setNegative (false);

    	// Get the points associated with the planar surface
    	extract.filter (*cloud_plane);
    	pointCloudVisualizer(cloud_plane, 'r', "Ground plane removed" + boost::lexical_cast<string>(j));

		*/
    	// Remove the planar inliers, extract the rest
    	extract.setNegative (true);
    	extract.filter (*processCloud);
    	j++;
  	}
}


void PCLPoleDetector::preProcessor(double meanKNoise, double stdDevNoise, double distThreshold){
  	// Filtering the point cloud to remove outliers
  	// The mean and stdDev values have to be tuned for the particular case
  	statisticalOutlierRemover(meanKNoise, stdDevNoise);
  	pcl::PointXYZ minPt , maxPt;
  	pcl::getMinMax3D(*processCloud, minPt, maxPt);

  	//pointCloudVisualizer(processCloud, 'g', "Noise removed cloud");
  	cerr << "PointCloud size after noise removal: " << processCloud->width * processCloud->height << " data points." << endl;
  	groundPlaneRemover(distThreshold);
  	cerr << "PointCloud size after ground plane removal: " << processCloud->width * processCloud->height << " data points." << endl;
}

void PCLPoleDetector::DONBuilder(pcl::PointCloud<pcl::PointNormal>::Ptr donCloud, double scaleSmall, double scaleLarge){
	// Create a search tree, use KDTreee for non-organized data.
  	pcl::search::Search<pcl::PointXYZ>::Ptr tree;
  	if (processCloud->isOrganized ()){
    	tree.reset (new pcl::search::OrganizedNeighbor<pcl::PointXYZ> ());
  	}
  	else{
    	tree.reset (new pcl::search::KdTree<pcl::PointXYZ> (false));
  	}

  	// Set the input pointcloud for the search tree
  	tree->setInputCloud (processCloud);

  	if (scaleSmall >= scaleLarge){
    	cerr << "Error: Large scale must be > small scale!" << endl;
    	exit (EXIT_FAILURE);
  	}

  	// Compute normals using both small and large scales at each point
  	pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::PointNormal> ne;
  	ne.setInputCloud (processCloud);
  	ne.setSearchMethod (tree);

  	/**
   	* NOTE: setting viewpoint is very important, so that we can ensure
   	* normals are all pointed in the same direction!
   	*/
  	ne.setViewPoint (std::numeric_limits<float>::max (), std::numeric_limits<float>::max (), std::numeric_limits<float>::max ());

  	// calculate normals with the small scale
  	cout << "Calculating normals for scale..." << scaleSmall << endl;
  	pcl::PointCloud<pcl::PointNormal>::Ptr normals_small_scale (new pcl::PointCloud<pcl::PointNormal>);

  	ne.setRadiusSearch (scaleSmall);
  	ne.compute (*normals_small_scale);

  	// calculate normals with the large scale
  	cout << "Calculating normals for scale..." << scaleLarge << endl;
  	pcl::PointCloud<pcl::PointNormal>::Ptr normals_large_scale (new pcl::PointCloud<pcl::PointNormal>);

  	ne.setRadiusSearch (scaleLarge);
  	ne.compute (*normals_large_scale);

  	cout << "Calculating DoN... " << endl;
  	// Create DoN operator
  	pcl::DifferenceOfNormalsEstimation<pcl::PointXYZ, pcl::PointNormal, pcl::PointNormal> don;
  	don.setInputCloud (processCloud);
  	don.setNormalScaleLarge (normals_large_scale);
  	don.setNormalScaleSmall (normals_small_scale);

  	if (!don.initCompute ())
  	{
    	std::cerr << "Error: Could not intialize DoN feature operator" << std::endl;
    	exit (EXIT_FAILURE);
  	}

  	// Compute DoN
  	don.computeFeature (*donCloud);

  	/* To write DON cloud to file, uncomment below
  	// Save DoN features
  	pcl::PCDWriter writer;
 	writer.write<pcl::PointNormal> ("don.pcd", *donCloud, true);
	*/
}

void PCLPoleDetector::DONThresholder(pcl::PointCloud<pcl::PointNormal>::Ptr donCloud, double thresholdDON){
	// Implementing the thesholding of DON
	// Build the condition for filtering
	pcl::ConditionOr<pcl::PointNormal>::Ptr range_cond (
	new pcl::ConditionOr<pcl::PointNormal> ()
	);
	range_cond->addComparison (pcl::FieldComparison<pcl::PointNormal>::ConstPtr (
	                       new pcl::FieldComparison<pcl::PointNormal> ("curvature", pcl::ComparisonOps::GT, thresholdDON))
	                     );
	// Build the filter
	pcl::ConditionalRemoval<pcl::PointNormal> condrem;
	condrem.setCondition(range_cond);
	condrem.setInputCloud (donCloud);

	// Apply filter
	condrem.filter (*donCloud);


	std::cout << "Filtered Pointcloud: " << donCloud->points.size () << " data points." << std::endl;
	// Save filtered output
	//*
	pcl::PCDWriter writer;
	writer.write<pcl::PointNormal> ("don_filtered.pcd", *donCloud, true);
	//*/

}

void PCLPoleDetector::euclideanClusterExtractor(vector<pcl::PointIndices> &clusterIndices, double minClusterSize, double maxClusterSize, double clusterTolerance){
	// Creating the KdTree object for the search method of the extraction
  	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
  	tree->setInputCloud (processCloud);

  	pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
  	ec.setClusterTolerance (clusterTolerance);
  	ec.setMinClusterSize (minClusterSize);
  	ec.setMaxClusterSize (maxClusterSize);
  	ec.setSearchMethod (tree);
  	ec.setInputCloud (processCloud);
  	ec.extract (clusterIndices);
}

void PCLPoleDetector::euclideanClusterExtractor(pcl::PointCloud<pcl::PointNormal>::Ptr donCloud, vector<pcl::PointIndices> &clusterIndices, double minClusterSize, double maxClusterSize, double clusterTolerance){
	// Creating the KdTree object for the search method of the extraction
  	pcl::search::KdTree<pcl::PointNormal>::Ptr tree (new pcl::search::KdTree<pcl::PointNormal>);
  	tree->setInputCloud (donCloud);

  	pcl::EuclideanClusterExtraction<pcl::PointNormal> ec;
  	ec.setClusterTolerance (clusterTolerance);
  	ec.setMinClusterSize (minClusterSize);
  	ec.setMaxClusterSize (maxClusterSize);
  	ec.setSearchMethod (tree);
  	ec.setInputCloud (donCloud);
  	ec.extract (clusterIndices);
}


void PCLPoleDetector::clusterFilter(vector<pcl::PointIndices> const &clusterIndices, double maxDiameter){
  	int j = 0;
  	boost::random::uniform_int_distribution<> dist(0, 255);
  	for (vector<pcl::PointIndices>::const_iterator it = clusterIndices.begin (); it != clusterIndices.end (); ++it){
  		//** Creating the point cloud for each cluster
    	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZ>);
    	for (vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); ++pit){
      		cloud_cluster->points.push_back (processCloud->points[*pit]); //*
    	}
    	cloud_cluster->width = cloud_cluster->points.size ();
    	cloud_cluster->height = 1;
    	cloud_cluster->is_dense = true;
    	//pointCloudVisualizer(cloud_cluster, 'b', "cluster" + boost::lexical_cast<string>(j));

    	//** Check if the cluster satisfies Bounding box max-Diameter criterion
    	pcl::PointXYZ minPt, maxPt;
    	Eigen::Vector4f minVec, maxVec;
    	pcl::getMinMax3D(*cloud_cluster, minPt, maxPt);
    	PointXYZ2eigenV4f2D(minVec, minPt);
    	PointXYZ2eigenV4f2D(maxVec, maxPt);
    	double clusterDiameter = (maxVec - minVec).norm();
    	//cerr << "Max Distance of cluster " << j << ": " << maxDistCluster << endl;
    	if (clusterDiameter <= maxDiameter){
    		Eigen::Vector4f vecCentroid;
    		pcl::compute3DCentroid(*cloud_cluster, vecCentroid);
    		//cerr << "Radius of cluster " << j << ": " << radius << endl;
    		//Cluster cluster(vecCentroid, radius);
    		filteredCluster.push_back(Cluster(vecCentroid, clusterDiameter/2, minPt, maxPt, cloud_cluster));

    		//* Uncomment to save the un-stitched clusters
    		//** Making color for each cluster
	    	uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
			uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
			//**
    		for (size_t i = 0; i < cloud_cluster->points.size(); ++i){
    			pcl::PointXYZRGB PtColored;
      			makeColoredPoint(PtColored, cloud_cluster->points[i], rgb);
      			clusterCloud->points.push_back(PtColored);
    		}
    		//*/
    		j++;
    	}
   	}
}

void PCLPoleDetector::clusterCloudBuilder(vector<pcl::PointIndices> const &clusterIndices){
  	boost::random::uniform_int_distribution<> dist(0, 255);
  	for (vector<pcl::PointIndices>::const_iterator it = clusterIndices.begin (); it != clusterIndices.end (); ++it){
  		//** Creating the point cloud for each cluster
    	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZ>);
    	for (vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); ++pit){
      		cloud_cluster->points.push_back (processCloud->points[*pit]); //*
    	}
    	cloud_cluster->width = cloud_cluster->points.size ();
    	cloud_cluster->height = 1;
    	cloud_cluster->is_dense = true;

    	pcl::PointXYZ minPt, maxPt;
    	Eigen::Vector4f minVec, maxVec;
    	pcl::getMinMax3D(*cloud_cluster, minPt, maxPt);
    	PointXYZ2eigenV4f2D(minVec, minPt);
    	PointXYZ2eigenV4f2D(maxVec, maxPt);
    	double xyDiameter = (maxVec - minVec).norm();

    	Eigen::Vector4f vecCentroid;
    	pcl::compute3DCentroid(*cloud_cluster, vecCentroid);
		filteredCluster.push_back(Cluster(vecCentroid, xyDiameter/2, minPt, maxPt, cloud_cluster));

		/* Uncomment to save the un-stitched clusters
		//** Making color for each cluster
    	uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
		uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);

		for (size_t i = 0; i < cloud_cluster->points.size(); ++i){
			pcl::PointXYZRGB PtColored;
  			makeColoredPoint(PtColored, cloud_cluster->points[i], rgb);
  			clusterCloud->points.push_back(PtColored);
		}
		//*/
    }
}

void PCLPoleDetector::segmenterDON(double minPts, double maxPts, double scaleSmall, double scaleLarge, double thresholdDON){
	// Create output cloud for DoN results
  	//pcl::PointCloud<pcl::PointNormal>::Ptr donCloud (new pcl::PointCloud<pcl::PointNormal>);
  	//pcl::copyPointCloud<pcl::PointXYZ, pcl::PointNormal>(*processCloud, *donCloud);
  	//DONBuilder(donCloud, scaleSmall, scaleLarge);
	pcl::PointCloud<pcl::PointNormal>::Ptr donCloud(new pcl::PointCloud<pcl::PointNormal>);
  	donCloud = _donCloud;

  	DONThresholder(donCloud, thresholdDON);
  	pcl::copyPointCloud<pcl::PointNormal, pcl::PointXYZ>(*donCloud, *processCloud);

  	vector<pcl::PointIndices> clusterIndices;
  	euclideanClusterExtractor(donCloud, clusterIndices, minPts, maxPts, scaleSmall);

	clusterCloudBuilder(clusterIndices);
	cerr << "Number of clusters found: " << filteredCluster.size() << endl;
}

void PCLPoleDetector::segmenterSingleCut(double minPts, double maxPts, double clusterTolerance, double maxDiameter){
	vector<pcl::PointIndices> clusterIndices;

	cerr << "Size of preProcessed cloud: " << processCloud->width * processCloud->height << " data points." << endl;
	euclideanClusterExtractor(clusterIndices, minPts, maxPts, clusterTolerance);
	cerr << "Number of clusters found: " << clusterIndices.size() << endl;

	//For post-filtering. To remove very large unprobable clusters
	clusterFilter(clusterIndices, maxDiameter);
	cerr << "Number of clusters after filtering: " << filteredCluster.size() << endl;
}


void PCLPoleDetector::clusterStitcher(double angleToVertical, double maxDistanceStitches){
	boost::random::uniform_int_distribution<> dist(0, 255);
	filteredCluster.sort(compareClusterHeight);

	/* To check the input clusters
	int j = 0;
	for (list<Cluster>::iterator it = filteredCluster.begin(); it != filteredCluster.end(); ++it){
		Cluster cluster = *it;
		cerr << "Radius: " << cluster.getRadius() << endl;
		cerr << "Z min: " << cluster.getZMin() << endl;
		cerr << "Z max: " << cluster.getZMax() << endl;
		pointCloudVisualizer(cluster.getClusterCloud(), "cluster cloud " + boost::lexical_cast<string>(j));
		pointCloudVisualizer(cluster, "cluster shape " + boost::lexical_cast<string>(j));
		j++;
		break;
	}
	*/

	int numClusters = 0;
	for (list<Cluster>::iterator it = filteredCluster.begin(); it != filteredCluster.end(); ++it){
		if (! it->isProcessed()){
			it->markProcessed();
			Segment stitchCluster;
			stitchCluster.addCluster(*it);
			//pointCloudVisualizer(it->getClusterCloud(), "cluster cloud " + boost::lexical_cast<string>(j));
			//pointCloudVisualizer(*it, "cluster shape " + boost::lexical_cast<string>(j));
			for (list<Cluster>::iterator it2 = filteredCluster.begin(); it2 != filteredCluster.end(); ++it2){
				if (! it2->isProcessed()){
					//pointCloudVisualizer(it2->getClusterCloud(), "cluster cloud " + boost::lexical_cast<string>(j));
					//pointCloudVisualizer(*it2, "cluster shape " + boost::lexical_cast<string>(j));
					double heightDiff = it2->getZMin() - stitchCluster.getZMax();
					//double heightDiff = it2->getCentroid()[2] - stitchCluster.getCentroid()[2];
					//double heightDiff = it2->getCentroid()[2] - stitchCluster.getZMax();
					if (heightDiff < maxDistanceStitches && heightDiff > 0){
						Eigen::Vector4f diffVec = it2->getCentroid() - stitchCluster.getCentroid();
						//** Hardcoded value below for Euclidean distance threshold of clusters to be stitched
						if (diffVec.norm() < 5){
							//double angle = abs(pcl::normAngle(diffVec.dot(Eigen::Vector4f::UnitZ())/diffVec.norm()));
							double angle = pcl::getAngle3D(diffVec, Eigen::Vector4f::UnitZ());
							if (angle < angleToVertical){
								/* Debugs
								cerr << "heightDiff: " << heightDiff << endl;
								cerr << "Euclidean distance: " << diffVec.norm() << endl;
								cerr << "Angle: " << angle << endl;
								*/
								it2->markProcessed();
								stitchCluster.addCluster(*it2);
							}
						}
					}
				}
			}
			stitchedClusters.push_back(stitchCluster);
			/* Uncomment to plot stitched cloud
			//** Making single color for each stitched cluster
			uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
			uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
			for (list<Cluster>::iterator it = stitchCluster.getSegmentParts().begin(); it != stitchCluster.getSegmentParts().end(); ++it){
				for (size_t i = 0; i < it->getClusterCloud()->points.size(); ++i){
					pcl::PointXYZRGB PtColored;
  					makeColoredPoint(PtColored, it->getClusterCloud()->points[i], rgb);
  					stitchedCloud->points.push_back(PtColored);
				}
			}
			//*/
		}
	}
	cerr << "Number of clusters after stitching: " << stitchedClusters.size() << endl;
}

void PCLPoleDetector::poleDetector(double minPoleHeight, double xyBoundThreshold){
	boost::random::uniform_int_distribution<> dist(0, 255);
	for (list<Segment>::iterator it = stitchedClusters.begin(); it != stitchedClusters.end(); ++it){
		Segment candidatePole = *it;
		if (candidatePole.getHeight() > minPoleHeight){
			Eigen::Vector4f minVecX, maxVecX, minVecY, maxVecY;
			pcl::PointXYZ minPtX, maxPtX, minPtY, maxPtY;
			minPtY = minPtX = candidatePole.getMinPt();
			maxPtY = maxPtX = candidatePole.getMaxPt();
			minPtX.y = (minPtX.y + maxPtX.y)/2;
			maxPtX.y = minPtX.y;
			PointXYZ2eigenV4f2D(minVecX, minPtX);
	    	PointXYZ2eigenV4f2D(maxVecX, maxPtX);
			minPtY.x = (minPtY.x + maxPtY.x)/2;
			maxPtY.x = minPtY.x;
			PointXYZ2eigenV4f2D(minVecY, minPtY);
	    	PointXYZ2eigenV4f2D(maxVecY, maxPtY);

	    	double xyBound = max((maxVecX - minVecX).norm(), (maxVecY - minVecY).norm());
	    	if (xyBound <= xyBoundThreshold){
				detectedPoles.push_back(candidatePole);
				//* Uncomment to plot single cloud
				//** Making single color for each detected pole
				uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
				uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
				for (list<Cluster>::iterator it = candidatePole.getSegmentParts().begin(); it != candidatePole.getSegmentParts().end(); ++it){
					for (size_t i = 0; i < it->getClusterCloud()->points.size(); ++i){
						pcl::PointXYZRGB PtColored;
	  					makeColoredPoint(PtColored, it->getClusterCloud()->points[i], rgb);
	  					poleCloud->points.push_back(PtColored);
					}
				}
				//*/
			}
		}
	}
	cerr << "Number of detected pole like structures: " << detectedPoles.size() << endl;
}


void PCLPoleDetector::treeExtractor(double minTreeHeight, double xyBoundMin, double xyBoundMax){
	boost::random::uniform_int_distribution<> dist(0, 255);
	for (list<Segment>::iterator it = stitchedClusters.begin(); it != stitchedClusters.end(); ++it){
		Segment candidateTree = *it;
		if (candidateTree.getHeight() > minTreeHeight){
			Eigen::Vector4f minVecX, maxVecX, minVecY, maxVecY;
			pcl::PointXYZ minPtX, maxPtX, minPtY, maxPtY;
			minPtY = minPtX = candidateTree.getMinPt();
			maxPtY = maxPtX = candidateTree.getMaxPt();
			minPtX.y = (minPtX.y + maxPtX.y)/2;
			maxPtX.y = minPtX.y;
			PointXYZ2eigenV4f2D(minVecX, minPtX);
	    	PointXYZ2eigenV4f2D(maxVecX, maxPtX);
			minPtY.x = (minPtY.x + maxPtY.x)/2;
			maxPtY.x = minPtY.x;
			PointXYZ2eigenV4f2D(minVecY, minPtY);
	    	PointXYZ2eigenV4f2D(maxVecY, maxPtY);

	    	double xyBound = max((maxVecX - minVecX).norm(), (maxVecY - minVecY).norm());
	    	if (xyBound <= xyBoundMax && xyBound >= xyBoundMin){
				extractedTrees.push_back(candidateTree);
				//* Uncomment to plot single cloud
				//** Making single color for each detected pole
				uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
				uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
				for (list<Cluster>::iterator it = candidateTree.getSegmentParts().begin(); it != candidateTree.getSegmentParts().end(); ++it){
					for (size_t i = 0; i < it->getClusterCloud()->points.size(); ++i){
						pcl::PointXYZRGB PtColored;
	  					makeColoredPoint(PtColored, it->getClusterCloud()->points[i], rgb);
	  					poleCloud->points.push_back(PtColored);
					}
				}
				//*/
			}
		}
	}
	cerr << "Number of detected tree like structures: " << extractedTrees.size() << endl;
}


void PCLPoleDetector::treeExtractor(double maxDistanceTrees){
	boost::random::uniform_int_distribution<> dist(0, 255);
	list<Segment>::iterator it, it2;
	it = detectedPoles.begin();
	int j = 1;
	while(it != detectedPoles.end()){
		Segment candidateTree = *it;
		bool tree = false;
		it2 = detectedPoles.begin();
		advance(it2, j);
		while (it2!= detectedPoles.end()){
			double diffDistance = (candidateTree.getCentroid() - it2->getCentroid()).norm();
			if (diffDistance < maxDistanceTrees){
				tree = true;
				candidateTree.mergeSegment(*it2);
				it2 = detectedPoles.erase(it2);
			}
			else
				++it2;
		}
		if (tree){
			extractedTrees.push_back(candidateTree);
			//* Uncomment to plot trees
			//** Making single color for each detected pole
			uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
			uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
			for (list<Cluster>::iterator it3 = candidateTree.getSegmentParts().begin(); it3 != candidateTree.getSegmentParts().end(); ++it3){
				for (size_t i = 0; i < it3->getClusterCloud()->points.size(); ++i){
					pcl::PointXYZRGB PtColored;
	  				makeColoredPoint(PtColored, it3->getClusterCloud()->points[i], rgb);
	  				stitchedCloud->points.push_back(PtColored);
				}
			}
			//*/
			it = detectedPoles.erase(it);
		}
		else{
			//* Uncomment to plot poles
			//** Making single color for each detected pole
			uint8_t r = dist(randomGen), g = dist(randomGen), b = dist(randomGen);
			uint32_t rgb = ((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b);
			for (list<Cluster>::iterator it3 = candidateTree.getSegmentParts().begin(); it3 != candidateTree.getSegmentParts().end(); ++it3){
				for (size_t i = 0; i < it3->getClusterCloud()->points.size(); ++i){
					pcl::PointXYZRGB PtColored;
	  				makeColoredPoint(PtColored, it3->getClusterCloud()->points[i], rgb);
	  				poleCloud->points.push_back(PtColored);
				}
			}
			//*/
			++it;
			++j;
		}
	}
	cerr << "Number of detected Poles: " << detectedPoles.size() << endl;
	cerr << "Number of detected Trees: " << extractedTrees.size() << endl;
}


void PCLPoleDetector::algorithmSingleCut(string pathToPCDFile, double xyBoundThreshold, double maxDistanceStitches, double minPoleHeight, double scaleSmall){
	//readPCD(pathToPCDFile);
	readDON(pathToPCDFile);
	double meanKNoise = 10;
	double stdDevNoise = 1;
	double distThresholdGround = 0.02;
	// preProcessor(meanKNoise, stdDevNoise, distThresholdGround);

	double maxPts = 50000;
	double minPts = 15;
	// double distThresholdCluster = 0.3;
	// double maxDiameter = 1;
	// segmenterSingleCut(minPts, maxPts, distThresholdCluster, maxDiameter);

	double thresholdDON = 0.25;
	double scaleLarge = scaleSmall*10;
	segmenterDON(minPts, maxPts, scaleSmall, scaleLarge, thresholdDON);


	double angleToVertical = 0.35;
	clusterStitcher(angleToVertical, maxDistanceStitches);
	poleDetector(minPoleHeight, xyBoundThreshold);
	//treeExtractor(maxDistanceTrees);

	writePCD("output_pcd.pcd");
	/*
	while (!viewer->wasStopped ()) { // Display the visualiser until 'q' key is pressed
    	viewer->spinOnce (100);
    	boost::this_thread::sleep (boost::posix_time::microseconds (100000));
	}
	*/
}

void PCLPoleDetector::buildRefClusters(string pathToPCDFile, double maxDistanceStitches, double scaleSmall){
	readDON(pathToPCDFile);
	double maxPts = 50000;
	double minPts = 15;
	double thresholdDON = 0.25;
	double scaleLarge = scaleSmall*10;
	segmenterDON(minPts, maxPts, scaleSmall, scaleLarge, thresholdDON);
	double angleToVertical = 0.35;
	clusterStitcher(angleToVertical, maxDistanceStitches);
	double minPoleHeight = 3;
	double xyBoundMin = 2;
	double xyBoundMax = 20;
	double xyBoundThreshold = 2;
	//treeExtractor(minPoleHeight, xyBoundMin, xyBoundMax);
	poleDetector(minPoleHeight, xyBoundThreshold);

	writePoles();
	writePCD("poles.pcd");
}
