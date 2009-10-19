/*
=========================================
Utils.cpp
=========================================
*/

#include "Misc/Utils.h"

#include "Objects/Misc/MessageBox.h"
#include "Worlds/Level.h"

namespace Util {

NGF::GameObject *showMessage(Ogre::String message, Ogre::Real time)
{
    return GlbVar.goMgr->createObject("MessageBox", Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY, NGF::PropertyList::create
            ("message", message, "")
            ("time", Ogre::StringConverter::toString(time))
            );
}

Ogre::String saveName(unsigned int worldNum)
{
    //Makes savefile name from world number of Level.
    Level *lvl = dynamic_cast<Level*>(GlbVar.woMgr->getWorld(worldNum));
    if (lvl)
        return lvl->getNgfName();
    return "NULL";
}

void screenshot(Ogre::String filename, const Ogre::String &extension)
{
    //Keep a count.
    static int count = -1;
    ++count;

    //Write it.
    filename = (USER_PREFIX "Screenshots/") + filename + Ogre::StringConverter::toString(count) + extension;
    GlbVar.ogreWindow->writeContentsToFile(filename);
}

void highResScreenshot(Ogre::RenderWindow* pRenderWindow, Ogre::Camera* pCamera, const int& pGridSize, Ogre::String pFileName, const Ogre::String& pFileExtension, const bool& pStitchGridImages)
{
    //Keep a count.
    static int count = -1;
    ++count;
    pFileName = (USER_PREFIX "Screenshots/") + pFileName + Ogre::StringConverter::toString(count);

    //Don't want mouse cursor showing up in the shot.
    GlbVar.gui->hidePointer();

    Ogre::String gridFilename;

    if(pGridSize <= 1)
    {
        // Simple case where the contents of the screen are taken directly
        // Also used when an invalid value is passed within pGridSize (zero or negative grid size)
        gridFilename = pFileName + pFileExtension;

        pRenderWindow->writeContentsToFile(gridFilename);
    }
    else
    {
        // Generate a grid of screenshots
        pCamera->setCustomProjectionMatrix(false); // reset projection matrix
        Ogre::Matrix4 standard = pCamera->getProjectionMatrix();
        double nearDist = pCamera->getNearClipDistance();
        double nearWidth = (pCamera->getWorldSpaceCorners()[0] - pCamera->getWorldSpaceCorners()[1]).length();
        double nearHeight = (pCamera->getWorldSpaceCorners()[1] - pCamera->getWorldSpaceCorners()[2]).length();
        Ogre::Image sourceImage;
        Ogre::uchar* stitchedImageData;

        // Process each grid
        for (int nbScreenshots = 0; nbScreenshots < pGridSize * pGridSize; nbScreenshots++) 
        { 
            // Use asymmetrical perspective projection.
            int y = nbScreenshots / pGridSize; 
            int x = nbScreenshots - y * pGridSize; 
            Ogre::Matrix4 shearing( 
                    1, 0,(x - (pGridSize - 1) * 0.5) * nearWidth / nearDist, 0, 
                    0, 1, -(y - (pGridSize - 1) * 0.5) * nearHeight / nearDist, 0, 
                    0, 0, 1, 0, 
                    0, 0, 0, 1); 
            Ogre::Matrix4 scale( 
                    pGridSize, 0, 0, 0, 
                    0, pGridSize, 0, 0, 
                    0, 0, 1, 0, 
                    0, 0, 0, 1); 
            pCamera->setCustomProjectionMatrix(true, standard * shearing * scale);
            Ogre::Root::getSingletonPtr()->_updateAllRenderTargets();
            gridFilename = pFileName + Ogre::StringConverter::toString(nbScreenshots) + pFileExtension;


            // Screenshot of the current grid
            pRenderWindow->writeContentsToFile(gridFilename);

            if(pStitchGridImages)
            {
                // Automatically stitch the grid screenshots
                sourceImage.load(gridFilename, "General"); // Assumes that the current directory is within the "General" resource group
                int sourceWidth = (int) sourceImage.getWidth();
                int sourceHeight = (int) sourceImage.getHeight();
                Ogre::ColourValue colourValue;
                int stitchedX, stitchedY, stitchedIndex;

                // Allocate memory for the stitched image when processing the screenshot of the first grid
                if(nbScreenshots == 0)
                    stitchedImageData = new Ogre::uchar[(sourceImage.getWidth() * pGridSize) * (sourceImage.getHeight() * pGridSize) * 3]; // 3 colors per pixel

                // Copy each pixel within the grid screenshot to the proper position within the stitched image
                for(int rawY = 0; rawY < sourceHeight; ++rawY)
                {
                    for(int rawX = 0; rawX < sourceWidth; ++rawX)
                    {
                        colourValue = sourceImage.getColourAt(rawX, rawY, 0);
                        stitchedX = x * sourceWidth + rawX;
                        stitchedY = y * sourceHeight + rawY;
                        stitchedIndex = stitchedY * sourceWidth * pGridSize + stitchedX;
                        Ogre::PixelUtil::packColour(colourValue,
                                Ogre::PF_R8G8B8,
                                (void*) &stitchedImageData[stitchedIndex * 3]);
                    }
                }
                // The screenshot of the grid is no longer needed
                remove(gridFilename.c_str());
            }
        } 
        pCamera->setCustomProjectionMatrix(false); // reset projection matrix 

        if(pStitchGridImages)
        {
            // Save the stitched image to a file
            Ogre::Image targetImage;
            targetImage.loadDynamicImage(stitchedImageData,
                    sourceImage.getWidth() * pGridSize,
                    sourceImage.getHeight() * pGridSize,
                    1, // depth
                    Ogre::PF_R8G8B8,
                    false);
            targetImage.save(pFileName + pFileExtension);
            delete[] stitchedImageData;
        }
    }

    GlbVar.gui->showPointer();
}

}
