/*
 * =====================================================================================
 *
 *       Filename:  PythonBinds.cpp
 *
 *    Description:  The GraLL2-Python library.
 *
 *        Created:  06/11/2009 09:54:33 PM
 *       Compiler:  gcc
 *
 *         Author:  Nikhilesh (nikki)
 *
 * =====================================================================================
 */

#include "Globals.h"

//--- Wrapper functions ---------------------------------------------------------

//Show message box.
NGF::Python::PythonObjectConnectorPtr py_showMessage(Ogre::String message, Ogre::Real time)
{
    NGF::GameObject *msg = Util::showMessage(message, time);
    NGF::Python::PythonGameObject *pyMsg = dynamic_cast<NGF::Python::PythonGameObject*>(msg);
    return pyMsg->getConnector();
}

//World switching.
void py_nextWorld() 
{ 
    Util::nextWorld();
}
void py_previousWorld() 
{ 
    Util::previousWorld(); 
}
void py_gotoWorld(int index) 
{ 
    Util::gotoWorld(index); 
}

//Dimension stuff (if you want to switch dimensions, best you do it through the 
//Player object).
void py_switchDimension() 
{ 
    GlbVar.dimMgr->switchDimension(); 
}
void py_setDimension(int dimension) 
{ 
    GlbVar.dimMgr->setDimension(dimension); 
}

//Fades.
void py_fadeInScreen(Ogre::Real time) 
{ 
    GlbVar.fader->fadeIn(time); 
}
void py_fadeOutScreen(Ogre::Real time) 
{ 
    GlbVar.fader->fadeOut(time); 
}
void py_fadeInOutScreen(Ogre::Real in, Ogre::Real pause, Ogre::Real out) 
{ 
    GlbVar.fader->fadeInOut(in, pause, out); 
}

//Level stuff.
bool py_saveExists(unsigned int levelNum)
{
    return Util::getRecordFromLevelNum(levelNum).checkpoint;
}
void py_removeSave(unsigned int levelNum)
{
    Util::getRecordFromLevelNum(levelNum).checkpoint = false;
}
unsigned int py_getCurrentLevelNum()
{
    return Util::worldNumToLevelNum(GlbVar.woMgr->getCurrentWorldIndex());
}

//Music stuff.
void py_playMusic(Ogre::String music)
{
    GlbVar.musicMgr->playMusic(music);
}
void py_crossFadeMusic(Ogre::String music, Ogre::Real time)
{
    GlbVar.musicMgr->crossFade(music, time);
}
void py_setMusicGain(Ogre::Real gain)
{
    GlbVar.musicMgr->setGain(gain);
}
Ogre::Real py_getMusicGain()
{
    return GlbVar.musicMgr->getGain();
}

//Video recording stuff.
void py_startRecordingVideo(Ogre::Real period)
{
    GlbVar.videoRec->startRecording(period);
}
void py_stopRecordingVideo()
{
    GlbVar.videoRec->stopRecording();
}

//Control setting.
void py_setControl(Ogre::String key, Ogre::String value)
{
    GlbVar.settings.controls.keys[key] = GlbVar.keyMap->stringToKey(value, OIS::KC_UNASSIGNED);
}
Ogre::String py_getControl(Ogre::String key)
{
    return GlbVar.keyMap->keyToString(GlbVar.settings.controls.keys[key]);
}

//--- The module ----------------------------------------------------------------

BOOST_PYTHON_MODULE(GraLL2)
{
    //Show message box.
    py::def("showMessage", py_showMessage);

    //Switch World.
    py::def("nextWorld", py_nextWorld);
    py::def("previousWorld", py_previousWorld);
    py::def("gotoWorld", py_gotoWorld);

    //Dimension stuff (if you want to switch dimensions, best you do it
    //through the Player object).
    py::def("switchDimension", py_switchDimension);
    py::def("setDimension", py_setDimension);

    //Fades.
    py::def("fadeInColour", py_fadeInScreen);
    py::def("fadeOutColour", py_fadeOutScreen);
    py::def("fadeInOutColour", py_fadeInOutScreen);

    //Level stuff.
    py::def("saveExists", py_saveExists);
    py::def("removeSave", py_removeSave);
    py::def("getCurrentLevelNum", py_getCurrentLevelNum);

    //Music stuff.
    py::def("playMusic", py_playMusic);
    py::def("crossFadeMusic", py_crossFadeMusic);
    py::def("setMusicGain", py_setMusicGain);
    py::def("getMusicGain", py_getMusicGain);

    //Video recording stuff.
    py::def("startRecordingVideo", py_startRecordingVideo);
    py::def("stopRecordingVideo", py_stopRecordingVideo);

    //Control setting.
    py::def("setControl", py_setControl);
    py::def("getControl", py_getControl);
}

//--- The function that gets called from Game::init() ---------------------------

void initPythonBinds()
{
    initGraLL2();
}

