/*
 * =====================================================================================
 *
 *       Filename:  Level.h
 *
 *    Description:  Usual level, does usual stuff. Handles loading of level file 
 *                  restoration of checkpoint state, music etc.
 *
 *        Created:  06/23/2009 03:09:46 PM
 *       Compiler:  gcc
 *
 *         Author:  Nikhilesh (nikki)
 *
 * =====================================================================================
 */

#ifndef __LEVEL_H__
#define __LEVEL_H__

#include "Globals.h"

class Level : 
    public NGF::World
{
    protected:
        Ogre::String mNgfName;
        Ogre::String mCaption;
        unsigned int mWorldNum;
        bool mUserLevel;
        bool mFirstFrame;

    public:
        Level(unsigned int worldNum, Ogre::String levelName, Ogre::String caption, bool userLevel = false);
        ~Level() {}

        void init();
        void tick(const Ogre::FrameEvent &evt);
        void stop();

        void startLevel();

        unsigned int getWorldNumber() { return mWorldNum; }
        Ogre::String getCaption() { return mCaption; }
        Ogre::String getNgfName() { return mNgfName; }
        void setCaption(const Ogre::String &caption) { mCaption = caption; }
        void setNgfName(const Ogre::String &ngfName) { mNgfName = ngfName; }
};

#endif
