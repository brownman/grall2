/*
=========================================
Director.cpp
=========================================
*/

#define __DIRECTOR_CPP__

#include "Objects/Misc/Director.h"

//--- NGF events ----------------------------------------------------------------
Director::Director(Ogre::Vector3 pos, Ogre::Quaternion rot, NGF::ID id, NGF::PropertyList properties, Ogre::String name)
    : NGF::GameObject(pos, rot, id , properties, name)
{
    addFlag("Director");

    //Python init event.
    NGF_PY_CALL_EVENT(init);

    //Create the Ogre stuff.
    mNode = GlbVar.ogreSmgr->getRootSceneNode()->createChildSceneNode(mOgreName, pos, rot);

    //Save properties.
    //mVelocity = Ogre::Vector3(0,0,-(Ogre::StringConverter::parseReal(mProperties.getValue("speed", 0, "2"))));
    mEnabled = Ogre::StringConverter::parseBool(mProperties.getValue("enabled", 0, "1"));
    mSpeed = Ogre::StringConverter::parseReal(mProperties.getValue("speed", 0, "2"));
    mDirection = rot;

    //Create the Physics stuff.
    mShape = new btBoxShape(btVector3(0.75,0.75,0.75));
    BtOgre::RigidBodyState *state = new BtOgre::RigidBodyState(mNode);
    mBody = new btRigidBody(0, state, mShape, btVector3(0,0,0));

    //Lots of flags. :-)
    initBody( (mEnabled ? DimensionManager::DIRECTOR : DimensionManager::NO_MOVING_CHECK)
            | DimensionManager::INVISIBLE 
            | DimensionManager::NO_DIM_CHECK 
            | DimensionManager::NO_CRATE_CHECK 
            | DimensionManager::NO_BULLET_HIT 
            | DimensionManager::DIM_BOTH
            );
    setBulletObject(mBody);
    mBody->setCollisionFlags(mBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
}
//-------------------------------------------------------------------------------
void Director::postLoad()
{
    //Python create event.
    NGF_PY_CALL_EVENT(create);
}
//-------------------------------------------------------------------------------
Director::~Director()
{
    //Python destroy event.
    NGF_PY_CALL_EVENT(destroy);

    //We only clear up stuff that we did.
    destroyBody();
    delete mShape;
}
//-------------------------------------------------------------------------------
void Director::unpausedTick(const Ogre::FrameEvent &evt)
{
    GraLL2GameObject::unpausedTick(evt);

    //Flip flags if needed.
    short int flags = mBody->getBroadphaseHandle()->m_collisionFilterGroup;
    if (mEnabled != (bool) (flags & DimensionManager::DIRECTOR))
    {
        flags ^= DimensionManager::DIRECTOR;
        flags ^= DimensionManager::NO_MOVING_CHECK;
        GlbVar.phyWorld->removeRigidBody(mBody);
        GlbVar.phyWorld->addRigidBody(mBody, flags, mDimensions);
    }
    
    //Python utick event.
    NGF_PY_CALL_EVENT(utick, evt.timeSinceLastFrame);
}
//-------------------------------------------------------------------------------
void Director::pausedTick(const Ogre::FrameEvent &evt)
{
    //Python ptick event.
    NGF_PY_CALL_EVENT(ptick, evt.timeSinceLastFrame);
}
//-------------------------------------------------------------------------------
NGF::MessageReply Director::receiveMessage(NGF::Message msg)
{
    switch (msg.code)
    {
        case MSG_GETDIRECTION:
            NGF_SEND_REPLY(mDirection);

        case MSG_GETSPEED:
            NGF_SEND_REPLY(mSpeed);

        case MSG_GETPOSITION:
            NGF_SEND_REPLY(mNode->getPosition());
    }
    return GraLL2GameObject::receiveMessage(msg);
}
//-------------------------------------------------------------------------------
void Director::collide(GameObject *other, btCollisionObject *otherPhysicsObject, btManifoldPoint &contact)
{
    if (!other)
        return;

    //Python collide event.
    NGF::Python::PythonGameObject *oth = dynamic_cast<NGF::Python::PythonGameObject*>(other);
    if (oth)
        NGF_PY_CALL_EVENT(collide, oth->getConnector());
}
//-------------------------------------------------------------------------------

//--- Python interface implementation -------------------------------------------
NGF_PY_BEGIN_IMPL(Director)
{
    NGF_PY_METHOD_IMPL(setOrientation)
    {
        mDirection = py::extract<Ogre::Quaternion>(args[0]);
        NGF_PY_RETURN();
    }

    NGF_PY_PROPERTY_IMPL(enabled, mEnabled, bool);
    NGF_PY_PROPERTY_IMPL(direction, mDirection, Ogre::Quaternion);
    NGF_PY_PROPERTY_IMPL(speed, mSpeed, Ogre::Real);
}
NGF_PY_END_IMPL_BASE(GraLL2GameObject)
//-------------------------------------------------------------------------------
