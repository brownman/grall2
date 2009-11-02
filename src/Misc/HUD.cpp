/*
=========================================
HUD.cpp
=========================================
*/

#include "Objects/Main/Player.h"

#include "Misc/HUD.h"

#define HUD_LAYER "Main"

#define TEXT_WIDTH 40 //Width of each text block.
#define TEXT_HEIGHT 50 //Height of each text block.

#define TEXT_SIDE_PAD 55 //Space between first text block and screen side edge.
#define TEXT_BOTTOM_PAD 50 //Space between text blocks and screen bottom edge.
#define TEXT_BETWEEN_PAD 10 //Space between each text block.

//Returns the position that the ID'th timer should have.
static inline Ogre::Vector2 timerPosition(int id)
{
    size_t winWidth = GlbVar.ogreWindow->getWidth();
    size_t winHeight = GlbVar.ogreWindow->getHeight();

    return Ogre::Vector2(
            winWidth - TEXT_SIDE_PAD - TEXT_WIDTH - ((TEXT_WIDTH + TEXT_BETWEEN_PAD) * id),
            winHeight - TEXT_BOTTOM_PAD - TEXT_HEIGHT
            );
}

//Returns the position that the i'th pickup display should have.
static inline Ogre::Vector2 pickupDisplayPosition(int i)
{
    size_t winHeight = GlbVar.ogreWindow->getHeight();

    return Ogre::Vector2(
            TEXT_SIDE_PAD + ((TEXT_WIDTH + TEXT_BETWEEN_PAD) * i),
            winHeight - TEXT_BOTTOM_PAD - TEXT_HEIGHT
            );
}

//Creates a MyGUI::StaticText for HUD.
static inline MyGUI::StaticTextPtr createStaticText(const MyGUI::IntCoord &coord, MyGUI::Align align, const Ogre::ColourValue &colour = Ogre::ColourValue::White)
{
    MyGUI::StaticTextPtr txt = GlbVar.gui->createWidget<MyGUI::StaticText>("StaticText", coord, MyGUI::Align::Default, HUD_LAYER);

    align |= MyGUI::Align::Bottom;
    txt->setTextAlign(align);
    txt->setFontName("HUD");
    txt->setTextColour(MyGUI::Colour(colour.r, colour.g, colour.b, colour.a));

    return txt;
}

//--- HUD -----------------------------------------------------------------------
HUD::HUD()
{
    size_t winHeight = GlbVar.ogreWindow->getHeight();
    Ogre::Vector2 pos = timerPosition(0);
    mBonusTimer = createStaticText(MyGUI::IntCoord(pos.x, pos.y, TEXT_WIDTH, TEXT_HEIGHT), MyGUI::Align::Right, Ogre::ColourValue::White);
}
//-------------------------------------------------------------------------------
void HUD::clear()
{
    //Remove all timers.
    for (TimerMap::iterator iter = mTimers.begin(); iter != mTimers.end(); ++iter)
        delete iter->second;
    mTimers.clear();

    //Remove all pickup displays.
    for (PickupDisplayMap::iterator iter = mPickupDisplays.begin(); 
            iter != mPickupDisplays.end(); ++iter)
        delete iter->second;
    mPickupDisplays.clear();

    //Clear bonus timer.
    mBonusTimer ->setCaption("");
}
//-------------------------------------------------------------------------------
void HUD::tick(const Ogre::FrameEvent &evt)
{
    //Update each timer, removing it if it's run out.
    for (TimerMap::iterator iter = mTimers.begin(); iter != mTimers.end(); )
    {
        HUDTimer *timer = iter->second;

        if (!timer->update(evt.timeSinceLastFrame))
        {
            delete iter->second;
            mTimers.erase(iter++);
        }
        else
            ++iter;
    }

    //Update each pickup display.
    {
        int i = 0;
        for (PickupDisplayMap::iterator iter = mPickupDisplays.begin(); 
                iter != mPickupDisplays.end(); ++i, ++iter)
            iter->second->update(pickupDisplayPosition(i));
    }

    //If there's a bonus time, show it, else don't.
    if (mBonusTimer)
        if (GlbVar.bonusTime > 0)
            mBonusTimer->setCaption(Ogre::StringConverter::toString((int) GlbVar.bonusTime));
        else
            mBonusTimer->setCaption("");
}
//-------------------------------------------------------------------------------
int HUD::addTimer(Ogre::Real time, const Ogre::ColourValue &colour)
{
    //Find a free ID. ID 0 would be bonus time.
    int id = 1;
    for (; mTimers.find(id) != mTimers.end(); ++id);

    mTimers.insert(std::make_pair(id, new HUDTimer(time, timerPosition(id), colour)));

    return id;
}
//-------------------------------------------------------------------------------
void HUD::removeTimer(int id)
{
    TimerMap::iterator iter = mTimers.find(id);

    if (iter != mTimers.end())
    {
        delete iter->second;
        mTimers.erase(iter);
    }
}
//-------------------------------------------------------------------------------
void HUD::addPickupDisplay(const Ogre::String &type, const Ogre::ColourValue &colour)
{
    //If we don't already have a timer for this type, add it.
    if (mPickupDisplays.find(type) == mPickupDisplays.end())
        mPickupDisplays.insert(std::make_pair(type, new PickupDisplay(type, colour)));
}
//-------------------------------------------------------------------------------
void HUD::removePickupDisplay(const Ogre::String &type)
{
    PickupDisplayMap::iterator iter = mPickupDisplays.find(type);

    if (iter != mPickupDisplays.end())
    {
        delete iter->second;
        mPickupDisplays.erase(iter);
    }
}
//-------------------------------------------------------------------------------

//--- HUDTimer ------------------------------------------------------------------
HUD::HUDTimer::HUDTimer(Ogre::Real time, Ogre::Vector2 pos, const Ogre::ColourValue &colour)
    : mTime(time),
      mText(0)
{
    mText = createStaticText(MyGUI::IntCoord(pos.x, pos.y, TEXT_WIDTH, TEXT_HEIGHT), MyGUI::Align::Right, colour);
    mText->setCaption(Ogre::StringConverter::toString((int) mTime));
}
//-------------------------------------------------------------------------------
HUD::HUDTimer::~HUDTimer()
{
    if (mText)
        GlbVar.gui->destroyWidget(mText);
}
//-------------------------------------------------------------------------------
bool HUD::HUDTimer::update(Ogre::Real elapsed)
{
    mTime -= elapsed;

    if (mTime <= 0)
        return false;

    mText->setCaption(Ogre::StringConverter::toString((int) mTime));

    return true;
}
//-------------------------------------------------------------------------------

//--- PickupDisplay -------------------------------------------------------------
HUD::PickupDisplay::PickupDisplay(const Ogre::String &type, const Ogre::ColourValue &colour)
    : mType(type)
{
    mText = createStaticText(MyGUI::IntCoord(0,0,0,0), MyGUI::Align::Left, colour);
    mText->setCaption(""); //Initially empty.
}
//-------------------------------------------------------------------------------
HUD::PickupDisplay::~PickupDisplay()
{
    GlbVar.gui->destroyWidget(mText);
}
//-------------------------------------------------------------------------------
void HUD::PickupDisplay::update(const Ogre::Vector2 &pos)
{
    mText->setCoord(MyGUI::IntCoord(pos.x, pos.y, TEXT_WIDTH, TEXT_HEIGHT));
    if (GlbVar.player)
    {
        Player *player = dynamic_cast<Player*>(GlbVar.player);
        if (player)
            mText->setCaption(Ogre::StringConverter::toString(player->getNumPickups(mType)));
    }
}
//-------------------------------------------------------------------------------
