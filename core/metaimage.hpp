#ifndef METAIMAGE_HPP
#define METAIMAGE_HPP

#include <vtkSmartPointer.h>
#include <vtkMetaImageReader.h>
#include <vtkImageData.h>
#include "ErrorHandler.hpp"

/**
 * A class to represent a MetaImage. This includes reading it and
 * knowing its position in space.
 * Region growing is also done by this class
 */
template<typename T>
class MetaImage {
public:
    /**
   * Constructor. Initialize everything to zero
   */
    MetaImage()
    {
        m_xsize = 0;
        m_ysize = 0;
        m_img = vtkSmartPointer<vtkImageData>::New();
        m_xspacing = 0.0;
        m_yspacing = 0.0;
        m_transform = Matrix4::Zero();
        m_idx = -1;
    }

    ~MetaImage()
    {

    }

    /**
   * @return the xsize in pixels
   */
    int
    getXSize() const
    {
        return m_xsize;
    }
    /**
   * @return the ysize in pixels
   */
    int
    getYSize() const
    {
        return m_ysize;
    }

    /**
   * @return the pixel spacing in x direction
   */
    double
    getXSpacing() const
    {
        return m_xspacing;
    }
    /**
   * @return the pixel spacing in y direction
   */
    double
    getYSpacing() const
    {
        return m_yspacing;
    }
    /**
   * Map a pixel position to an integer
   * @return an unique integer for the position x,y
   */
    int
    makehash(const int x, const int y) const
    {
        return x + getXSize()*y;
    }


    /**
   * @return the pointer to the pixel data
   */
    T*
    getPixelPointer()
    {
        return (T*)m_img->GetScalarPointer();
    }

    /**
   * @return the pointer to the pixel data
   */
    const T*
    getPixelPointer() const
    {
        return (T*)m_img->GetScalarPointer();
    }

    /**
   * Transform a point to from world coordinates to pixel coordinates
   * @param x The x coordinate is returned here
   * @param y The y coordinate is returned here
   * @param p The point to transform
   */
    void
    toImgCoords(double &x,
                double &y,
                const double p[3]) const
    {


        // Thing is, the matrices we are given are the opposite transform,
        // so we have to do the inverse transformation: p = R^Tp - R^Tp_0
        double offset_x, offset_y;
        x =
                m_transform(0,0)*p[0] +
                m_transform(1,0)*p[1] +
                m_transform(2,0)*p[2];
        // Now subtract the transformed offset
        offset_x =
                m_transform(0,0)*m_transform(0,3) +
                m_transform(1,0)*m_transform(1,3) +
                m_transform(2,0)*m_transform(2,3);
        x = (x - offset_x)/getXSpacing();
        y =
                m_transform(0,1)*p[0] +
                m_transform(1,1)*p[1] +
                m_transform(2,1)*p[2];
        // Now subtract the transformed offset
        offset_y =
                m_transform(0,1)*m_transform(0,3) +
                m_transform(1,1)*m_transform(1,3) +
                m_transform(2,1)*m_transform(2,3);

        y = (y - offset_y)/getYSpacing();


    }



    /**
   * Get the transformation matrix for this image
   * @return the transformation matrix
   * @sa setTransform
   */
    Matrix4
    getTransform()
    {
        return m_transform;
    }

    /**
   * Get the transformation matrix for this image
   * @return the transformation matrix
   * @sa setTransform
   */
    const Matrix4&
    getTransform() const
    {
        return m_transform;
    }

    /**
   * Perform region growing on this image
   * The Dt parameter specifies the data type of the return data (typically double)
   * @param ret The vector in which to store the points found
   * @param imgx The seed point (in pixel coordinates), X coordinate
   * @param imgy The seed point (in pixel coordinates), Y coordinate
   */
    template<typename Dt>
    void regionGrow(vector<Dt>& ret, int imgx, int imgy) const
    {

        vector<pair<int,int> > ptstack;

        // Get image data

        const T *imagedata = getPixelPointer();

        pair<int,int> cur;
        T curpt;

        // Initialize stack with seed point
        ptstack.push_back(make_pair(imgx, imgy));
        curpt = imagedata[imgx + imgy*getXSize()];

        ret.push_back(curpt);

        // Use a bool array instead of unordered_map, it's just faster even though we might need a little more memory
        vector<bool> visited;
        visited.resize(makehash( getXSize(), getYSize()));
        std::fill(visited.begin(), visited.end(), false);

        // Go!
        while(!ptstack.empty())
        {
            cur = ptstack.back();

            ptstack.pop_back();
            const int img_x = cur.first;
            const int img_y = cur.second;

            if(!inImage(img_x, img_y))
            {
                continue;
            }
            curpt = imagedata[img_x + img_y*getXSize()];

            // Is this point in?
            if(curpt != 0)
            {
                if(visited.at(makehash(img_x, img_y)) )
                    continue;

                ret.push_back(curpt);
                // Check the neighbours (4-connected)
                ptstack.insert(ptstack.end(),
                {   make_pair(img_x-1,img_y),
                    make_pair(img_x+1,img_y),
                    make_pair(img_x,img_y-1),
                    make_pair(img_x,img_y+1)});
            }
            visited.at(makehash(img_x, img_y)) = true;
        }

    }

    /**
   * Set the index of this image
   * @param i Index to set
   */
    inline
    void setIdx(int i)
    {
        m_idx = i;
    }

    /**
   * Get the index of this image
   * @return the index of this image
   */
    inline
    int getIdx() const
    {
        return m_idx;
    }


    /**
   * Factory function to get a bunch of images by reading them from disk.
   * @param prefix The prefix of the file name. File names are assumed to be of the format prefix$NUMBER.mhd
   * @return a vector containing the retrieved images
   */
    static vector<MetaImage>* readImages(const string & prefix)
    {
        // Images are on the format prefix$NUMBER.mhd
        vtkSmartPointer<vtkMetaImageReader> reader= vtkSmartPointer<vtkMetaImageReader>::New();
        vtkSmartPointer<ErrorObserver>  errorObserver =  vtkSmartPointer<ErrorObserver>::New();
        reader->AddObserver(vtkCommand::ErrorEvent,errorObserver);

        int i = 0;
        ostringstream ss;
        ss << prefix << i << ".mhd";
        string filename = ss.str();
        reader->SetFileName(filename.c_str());
        reader->Update();

        if(!reader->CanReadFile(filename.c_str())){
            cerr << filename.c_str() << endl;
            reportError("ERROR: Could not read velocity data \n");
        }

        vector<MetaImage> *ret = new vector<MetaImage>();
        while(reader->CanReadFile(filename.c_str()))
        {
            ret->push_back(MetaImage());
            ret->at(i).setIdx(i);
            ret->at(i).m_img->DeepCopy(reader->GetOutput());

#if VTK_MAJOR_VERSION <= 5
            ret->at(i).m_img->Update();
#else
#endif

            if (errorObserver->GetError())
            {
                reportError("ERROR: Could not read velocity data \n"+ errorObserver->GetErrorMessage());
            }
            if (errorObserver->GetWarning()){
                cerr << "Caught warning while reading velocity data! \n " << errorObserver->GetWarningMessage();
            }

            if ( reader->GetFileDimensionality() != 2){
                reportError("ERROR: Can only read 2-D data");
            }


            ret->at(i).m_xsize = reader->GetWidth();
            ret->at(i).m_ysize = reader->GetHeight();
            double *spacing;
            spacing = reader->GetPixelSpacing();
            ret->at(i).m_xspacing = spacing[0];
            ret->at(i).m_yspacing = spacing[1];

            std::ifstream infile(filename);
            std::string line;
            int found =0;
            int numToFind =2;
            while (std::getline(infile, line))
            {
                if(line.find("Offset")!=std::string::npos)
                {
                    string buf;
                    stringstream ss(line);
                    int k=0;
                    ss >> buf;
                    ss >> buf;
                    while (ss >> buf)
                    {
                        ret->at(i).m_transform(k++,3)=std::stod(buf);
                    }
                    ret->at(i).m_transform(3,3)=1;
                    found++;
                    if(found >=numToFind) break;

                }else if(line.find("TransformMatrix")!=std::string::npos)
                {
                    string buf;
                    stringstream ss(line);
                    int k=0;
                    int j=0;
                    ss >> buf;
                    ss >> buf;
                    while (ss >> buf)
                    {
                        if(k >2){
                            k=0;
                            j++;
                        }
                        ret->at(i).m_transform(k++,j)=std::stod(buf);
                    }
                    found++;
                    if(found >=numToFind) break;
                }
            }
            ss.clear();
            ss.str("");
            ss << prefix << ++i << ".mhd";
            filename = ss.str();
            reader->SetFileName(filename.c_str());
            reader->Update();
        }
        return ret;
    }

    /**
   * Test if a point is inside this image
   * @param img_x x coordinate (pixel space)
   * @param img_y y coordinate (pixel space)
   * @return true if point is inside the image, false otherwise
   */
    inline bool
    inImage(double img_x,double img_y) const
    {
        return !(img_x < 0
                 || img_x >= getXSize()
                 || img_y < 0
                 || img_y >= getYSize());
    }

private:
    vtkSmartPointer<vtkImageData> m_img;
    int m_idx;
    int m_xsize;
    int m_ysize;
    double m_xspacing;
    double m_yspacing;
    Matrix4 m_transform;
};

#endif //METAIMAGE_HPP
