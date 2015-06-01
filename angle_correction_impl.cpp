#include "spline3d.hpp"
#include "tpreader.hpp"
#include <vtkDoubleArray.h>
#include <sys/time.h>

using namespace std;


/**
 * Region grow all the images in all the intersections in all the splines
 * @param splines The splines to region grow
 */
void regionGrow(vector<Spline3D<D> > *splines){
  for(auto &spline: *splines)
  {
    for_each(spline.getIntersections().begin(), spline.getIntersections().end(),[](Intersection<D> &it){it.regionGrow();});
  }
}


/**
 * Get the time difference between two struct timevals
 * @param tv1 the first timeval
 * @param tv2 the second timeval
 * @return tv2 - tv1
 */
uint64_t getTimeDifference(struct timeval tv1, struct timeval tv2)
{
  uint64_t ret = (tv2.tv_sec-tv1.tv_sec)*1000000 + (  tv2.tv_usec - tv1.tv_usec);
  return ret;
}

/**
 * Convenience function to print a label and the difference between two struct timevals
 */
void printTime(string label, struct timeval tv1, struct timeval tv2)
{
  cerr << label << endl << "\t\t\t\t: " << (double)getTimeDifference(tv1, tv2)/1000.0 << " msecs" << endl;
}



vtkSmartPointer<vtkPolyData> flowDirection( vector<Spline3D<D> > *splines, double uncertainty_limit)
{
	vtkSmartPointer<vtkPoints> pointarray = vtkSmartPointer<vtkPoints>::New();

	vtkSmartPointer<vtkDoubleArray> flowdirection = vtkSmartPointer<vtkDoubleArray>::New();
	flowdirection->SetName("Flow direction");
	flowdirection->SetNumberOfComponents(3);


	vtkSmartPointer<vtkDoubleArray> dir_uncertainty = vtkSmartPointer<vtkDoubleArray>::New();
	dir_uncertainty->SetName("FLow direction uncertainty");

	vtkSmartPointer<vtkDoubleArray> velocitydata = vtkSmartPointer<vtkDoubleArray>::New();
	velocitydata->SetName("Vessel velocity");

	D p[3];
	D p_prev[3];
	D p_temp[3];
	D flow_vector[3];
	D flow_vector_n[3];
	D flow_direction;
	D abs_dir;
	D abs_vessel_vel;

	for(auto &spline: *splines)
	  {

		flow_direction = spline.getIntersections().getEstimatedDirection();
		abs_dir = flow_direction*sgn(flow_direction);
		abs_vessel_vel = spline.getIntersections().getEstimatedVelocity();
		abs_vessel_vel = abs_vessel_vel*sgn(abs_vessel_vel);


		if( abs_dir< uncertainty_limit){
					continue;
		}
		if(std::isnan(abs_dir) || std::isnan(abs_vessel_vel) ){
				continue;
		}

		spline.evaluateSingle(0,p_prev);
		for(double t = 0; t < (spline.length()-1); t+=0.3){
			spline.evaluateSingle(t,p);
			if(std::isnan(p[0]) || std::isnan(p[1]) || std::isnan(p[2])  ){
				continue;
			}

			p_temp[0]=p[0]-p_prev[0];
			p_temp[1]=p[2]-p_prev[1];
			p_temp[1]=p[2]-p_prev[2];

			if(length3d(p_temp)<1){
				continue;
			}

			p_prev[0]=p[0];
			p_prev[1]=p[1];
			p_prev[2]=p[2];

			spline.derivativeSingle(t,flow_vector);

			flow_vector[0]=flow_direction*flow_vector[0];
			flow_vector[1]=flow_direction*flow_vector[1];
			flow_vector[2]=flow_direction*flow_vector[2];

			normalize3d(flow_vector_n, flow_vector);
			if(std::isnan(flow_vector_n[0]) || std::isnan(flow_vector_n[1]) || std::isnan(flow_vector_n[2]) ){
				continue;
			}

			pointarray->InsertNextPoint(p);
			flowdirection->InsertNextTupleValue(flow_vector_n);
			dir_uncertainty->InsertNextTupleValue(&abs_dir);
			velocitydata->InsertNextTupleValue(&abs_vessel_vel);
		}

	  }


	 // Put it all inside a vtkPolyData

	  vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
	  vtkSmartPointer<vtkPointData> pointdata = polydata->GetPointData();
	  polydata->SetPoints(pointarray);

	  pointdata->AddArray(flowdirection);
	  pointdata->AddArray(dir_uncertainty);
	  pointdata->AddArray(velocitydata);

	  return polydata;

}


static vector<Spline3D<D> >*   angle_correction_impl(char* toolpositions, char* centerline, char* image_prefix, double Vnyq, double cutoff,  int nConvolutions)
{
	bool verbose = false;
  
  struct timeval tv1,tv2,tv3,tv4;
  gettimeofday(&tv3,NULL);

  // Read and build centerline spline  
  gettimeofday(&tv1,NULL);
  vector<Matrix4> matrices;
  TpReader tp_reader = TpReader(toolpositions);
  tp_reader.read();
  matrices = tp_reader.getMatrices();

  vtkSmartPointer<vtkPolyDataReader> clReader = vtkSmartPointer<vtkPolyDataReader>::New();
  
  clReader->SetFileName(centerline);
  clReader->Update();
  vtkPolyData *vpd_centerline = clReader->GetOutput();
 
  vector<Spline3D<D> > *splines = Spline3D<D>::build(vpd_centerline);
  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Build centerline", tv1, tv2);
  }
  
  // Read images
  gettimeofday(&tv1,NULL);
  vector<MetaImage<inData_t> > images = MetaImage<inData_t>::readImages(std::string(image_prefix),matrices);

  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Read images",tv1,tv2);
  }

  gettimeofday(&tv1,NULL);

  for(Spline3D<D>& spline : *splines)
  {
    spline.setTransform(true);
    spline.setAxis(1);
  }
  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Various initialization",tv1,tv2);
  }
  
  
  // Smooth the splines
  gettimeofday(&tv1,NULL);
  for(auto &spline: *splines)
  {
    for(int j = 0; j < nConvolutions; j++)
    {
      spline.applyConvolution({0.25, 0.50, 0.25 });
    }

  }
  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Smoothing", tv1, tv2);
  }
  
  // Compute control points for splines
  gettimeofday(&tv1, NULL);
  for(auto &spline: *splines)
  {
    spline.compute();
  }

  if (verbose)
  {
	  gettimeofday(&tv2, NULL);
	  printTime("Spline fitting", tv1, tv2);
  }


  // Find all the intersections
  gettimeofday(&tv1, NULL);
  int n_intersections = 0;
  for(auto &spline: *splines)
  {
    spline.findAllIntersections(images);
    spline.getIntersections().setVelocityEstimationCutoff(cutoff,1.0);
    n_intersections += spline.getIntersections().size();
  }
  gettimeofday(&tv2, NULL);
  if (verbose)
  {
	  printTime("Intersections",tv1, tv2);
	  cerr << "Found " << n_intersections << " intersections\n";

  }
  

  // Now that we know the intersection points, 
  // we can go through all the image planes and do the region growing.
  
  gettimeofday(&tv1, NULL);

  regionGrow(splines);
  
  if (verbose)
    {
	  gettimeofday(&tv2, NULL);
	  printTime("Region growing", tv1, tv2);
    }
  
  // We may now do the direction vector estimation
  gettimeofday(&tv1,NULL);
 
  for(auto &spline: *splines)
  {
    // Using the default parameters set in IntersectionSet constructor
    spline.getIntersections().estimateDirection();
  }

  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Direction estimation",tv1,tv2);
  }
 

  // Do aliasing correction
  if (Vnyq > 0){
	  gettimeofday(&tv1,NULL);
	  for(auto &spline: *splines)
	  {
		spline.getIntersections().correctAliasing(Vnyq);
	  }
	  if (verbose)
	  {
		  gettimeofday(&tv2,NULL);
		  printTime("Aliasing correction",tv1,tv2);
	  }
  }

  // Least squares velocity estimates
  gettimeofday(&tv1,NULL);
  for(auto &spline: *splines)
  {
    spline.getIntersections().estimateVelocityLS();
  }

  if (verbose)
  {
	  gettimeofday(&tv2,NULL);
	  printTime("Least Squares velocity estimate",tv1,tv2);
  }
 
  // Output direction and LS velocity
  if (verbose)
  {
	  int i = 0;
	  for(auto &spline: *splines)
	  {
		cerr << "Spline " << i++ << " gave direction "
		 << spline.getIntersections().getEstimatedDirection()
		 << " LS velocity " << spline.getIntersections().getEstimatedVelocity() << endl;
	  }

	  	  gettimeofday(&tv4,NULL);
	  printTime("Total",tv3,tv4);
  }

  return splines;
}
