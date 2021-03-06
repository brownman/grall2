/*
=========================================
Player.cpp
=========================================
*/

#define __PLAYER_CPP__

#include "Objects/Main/Player.h"

#include "BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"

#include "Objects/Misc/CameraHandler.h"
#include "Objects/Misc/Light.h"
#include "Objects/Misc/ParticleEffect.h"

#define PLAYER_TORQUE 7.77

//Makes sure the ghost object stays with us.
class PlayerMotionState : public BtOgre::RigidBodyState
{
    protected:
        btPairCachingGhostObject *mGhostObject;

    public:
        PlayerMotionState(Ogre::SceneNode *node, const btTransform &transform, const btTransform &offset, btPairCachingGhostObject *ghost)
            : BtOgre::RigidBodyState(node, transform, offset),
              mGhostObject(ghost)
        {
            mGhostObject->setWorldTransform(transform);
        }

        PlayerMotionState(Ogre::SceneNode *node, btPairCachingGhostObject *ghost)
            : BtOgre::RigidBodyState(node),
              mGhostObject(ghost)
        {
            mGhostObject->setWorldTransform(mTransform);
        }

        virtual void setWorldTransform(const btTransform &in) 
        {
            mGhostObject->setWorldTransform(in);
            BtOgre::RigidBodyState::setWorldTransform(in); //Let base do it's stuff.
        }
};

//--- NGF events ----------------------------------------------------------------
Player::Player(Ogre::Vector3 pos, Ogre::Quaternion rot, NGF::ID id, NGF::PropertyList properties, Ogre::String name)
    : NGF::GameObject(pos, rot, id , properties, name),
      mUnderControl(true),
      mDead(false),
      mLight(0),
      mInvincible(false), //This makes the game harder.
      mWon(false),
      mGravityTime(-1),
      mCanSwitchGravity(false),
      mGravityHUDId(-1)
{
    addFlag("Player");
    addFlag("Switcher");
    addFlag("Doorer");
    addFlag("OneWayer");

    //Python init event.
    NGF_PY_CALL_EVENT(init);

    //Do away with the previous fellow (if any).
    if (GlbVar.player)
        GlbVar.goMgr->destroyObject(GlbVar.player->getID());
    GlbVar.player = this;

    //Set the Python us.
    py::object &main = NGF::Python::PythonManager::getSingleton().getMainNamespace();
    py::exec(
            "import GraLL2\n\n"

            "def setPlayer(obj):\n"
            "  GraLL2.player = obj\n",
            main, main
            ); 
    main["setPlayer"](getConnector());
    py::exec(
            "del setPlayer\n",
            main, main
            );

    //Create the Ogre stuff.
    mEntity = GlbVar.ogreSmgr->createEntity(mOgreName, "Player.mesh");
    mEntity->setMaterialName("Objects/Player");
    mNode = GlbVar.ogreSmgr->getRootSceneNode()->createChildSceneNode(mOgreName, pos, rot);
    mNode->attachObject(mEntity);

    //Read in properties.
    mMinHeight = Ogre::StringConverter::parseReal(mProperties.getValue("minHeight", 0, "-4"));
    if (mMinHeight < -10)
        mMinHeight = -10;
    mCanSwitchDimensions = Ogre::StringConverter::parseBool(mProperties.getValue("canSwitchDimensions", 0, "yes"));

    //Create the ghost object for dimension tests (we do it before the actual body so that
    //we can pass it to the PlayerMotionState).
    mGhostObject = new btPairCachingGhostObject();

    //Create the Physics stuff.
    BtOgre::StaticMeshToShapeConverter converter(mEntity);
    mShape = converter.createSphere();
    btScalar mass = 2;
    btVector3 inertia;
    mShape->calculateLocalInertia(mass, inertia);
    PlayerMotionState *state = new PlayerMotionState(mNode, mGhostObject);

    btRigidBody::btRigidBodyConstructionInfo info(mass, state, mShape, inertia);
    info.m_friction = Ogre::Math::POS_INFINITY;
    info.m_linearDamping = 0.1;
    info.m_angularDamping = 0.6;

    mBody = new btRigidBody(info);
    mBody->setActivationState(DISABLE_DEACTIVATION);
    initBody( DimensionManager::PLAYER
            | DimensionManager::LIFTABLE
            | DimensionManager::MAGNETIC
            );
    setBulletObject(mBody);

    //Configure the GhostObject.
    mGhostShape = new btSphereShape(converter.getRadius() - 0.01);
    mGhostObject->setCollisionShape(mGhostShape);
    mGhostObject->setCollisionFlags(mGhostObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    GlbVar.phyWorld->addCollisionObject(mGhostObject
            , DimensionManager::NO_DIM_CHECK 
            | DimensionManager::NO_CRATE_CHECK
            | DimensionManager::NO_MOVING_CHECK
            | DimensionManager::NO_BULLET_HIT
            | DimensionManager::INVISIBLE
            | DimensionManager::DIM_BOTH, 
            DimensionManager::DIM_BOTH
            );

    //Control node is used for controlling the Player. He rotates in all kinds of crazy ways, but we need to know where we're headed.
    mControlNode = GlbVar.ogreSmgr->getRootSceneNode()->createChildSceneNode(mOgreName + "-controlnode", pos, rot);

    //Player can't be in a dimension that's not being displayed. :P
    setDimension(GlbVar.dimMgr->getCurrentDimension());

    //Make sounds.
    mDimSound = GlbVar.soundMgr->createSound(mOgreName + "_dimSound", "DimensionSwitch.wav", false, false);
    mNoDimSound = GlbVar.soundMgr->createSound(mOgreName + "_noDimSound", "NoDimensionSwitch.wav", false, false);
    mDimSound->setGain(0.4);
    mGravitySound = GlbVar.soundMgr->createSound(mOgreName + "_gravitySound", "GravitySwitch.wav", false, false);
    mGravitySound->setGain(0.55);
    mTimeUpSound = GlbVar.soundMgr->createSound(mOgreName + "_timeUpSound", "TimeUp.wav", false, false);

    mNode->attachObject(GlbVar.playerExplosionSound);
}
//-------------------------------------------------------------------------------
void Player::postLoad()
{
    //Do stuff we have to do when not deserialising.
    if (!(mProperties.getValue("NGF_SERIALISED", 0, "no") == "yes"))
    {
        if (!(mProperties.getValue("captureCameraHandler", 0, "yes") == "no"))
            captureCameraHandler();
        mLight = GlbVar.goMgr->createObject<Light>(mNode->getPosition(), Ogre::Quaternion::IDENTITY, NGF::PropertyList::create
                ("lightType", "point")
                ("colour", "0 0.3 0.75")
                ("specular", "0.1 0.1 0.1")
                ("attenuation", "10 0.6 0.4 0.6")
                );
        mLight->setPersistent(true); //We'll manage his destruction.
    }

    //Python create event.
    NGF_PY_CALL_EVENT(create);
}
//-------------------------------------------------------------------------------
Player::~Player()
{
    GlbVar.player = 0;

    //Python destroy event.
    NGF_PY_CALL_EVENT(destroy);

    //Destroy the light.
    if (mLight)
        GlbVar.goMgr->destroyObject(mLight->getID());

    //We only clear up stuff that we did.
    GlbVar.phyWorld->removeCollisionObject(mGhostObject);
    delete mGhostObject;
    delete mGhostShape;
    loseCameraHandler();
    destroyBody();
    delete mShape;

    GlbVar.ogreSmgr->destroySceneNode(mControlNode);
    mNode->detachAllObjects();

    GlbVar.soundMgr->destroySound(mDimSound);
    GlbVar.soundMgr->destroySound(mNoDimSound);
    GlbVar.soundMgr->destroySound(mGravitySound);
    GlbVar.soundMgr->destroySound(mTimeUpSound);

    GlbVar.ogreSmgr->destroyEntity(mEntity->getName());
}
//-------------------------------------------------------------------------------
void Player::unpausedTick(const Ogre::FrameEvent &evt)
{
    GraLL2GameObject::unpausedTick(evt);

    //Jump to displayed dimension.
    setDimension(GlbVar.dimMgr->getCurrentDimension());

    //Allow stuff to catch up.
    mControlNode->setPosition(mNode->getPosition());
    GlbVar.goMgr->sendMessage(mLight, NGF_MESSAGE(MSG_SETPOSITION, mNode->getPosition()));

    //Controls.
    if (mBody->getAngularVelocity().length2() < 600)
    {
        Ogre::Vector3 torque = Ogre::Vector3::ZERO;
        torque.x =  GlbVar.gravMgr->getSign() * (isKeyDown(GlbVar.settings.controls.keys["backward"]) - isKeyDown(GlbVar.settings.controls.keys["forward"]));
        torque.z =  (isKeyDown(GlbVar.settings.controls.keys["left"]) - isKeyDown(GlbVar.settings.controls.keys["right"]));
        torque *= PLAYER_TORQUE;

        mBody->applyTorque(BtOgre::Convert::toBullet(mControlNode->getOrientation() * torque));
    }

    OIS::MouseState ms = getMouseState();
    mControlNode->yaw(Ogre::Degree(GlbVar.gravMgr->getSign() * -ms.X.rel * GlbVar.settings.controls.turningSensitivity * 0.4));

    //Check whether we fell off.
    if (mBody->getWorldTransform().getOrigin().getY() < mMinHeight)
        die(false, true);

    //Update gravity timer.
    if (mGravityTime > 0)
    {
        mGravityTime -= evt.timeSinceLastFrame;

        if (mGravityTime <= 0)
        {
            mCanSwitchGravity = false;
            if (!GlbVar.gravMgr->isUp())
            {
                GlbVar.gravMgr->setUp(true); //Go back to normal.
                mTimeUpSound->stop();
                mTimeUpSound->play();
            }
        }
        else
            mCanSwitchGravity = true;
    }

    //Update HUD.
    if (mCanSwitchDimensions)
        if (GlbVar.dimMgr->getCurrentDimension() & 1)
            GlbVar.hud->setIcon("dimension", "Dimension1Icon.png");
        else
            GlbVar.hud->setIcon("dimension", "Dimension2Icon.png");
    else
        GlbVar.hud->removeIcon("dimension");

    if (mCanSwitchGravity)
        if (GlbVar.gravMgr->isUp())
            GlbVar.hud->setIcon("gravity", "GravityDownIcon.png");
        else
            GlbVar.hud->setIcon("gravity", "GravityUpIcon.png");
    else
        GlbVar.hud->removeIcon("gravity");

    //Log force.
    //Ogre::Vector3 torque = BtOgre::Convert::toOgre(mBody->getTotalTorque());
    //Ogre::Vector3 velocity = BtOgre::Convert::toOgre(mBody->getLinearVelocity());
    //Ogre::LogManager::getSingleton().logMessage("torque: " + Ogre::StringConverter::toString(torque));
    //Ogre::LogManager::getSingleton().logMessage("velocity: " + Ogre::StringConverter::toString(velocity));

    //Python utick event.
    NGF_PY_CALL_EVENT(utick, evt.timeSinceLastFrame);
}
//-------------------------------------------------------------------------------
void Player::pausedTick(const Ogre::FrameEvent &evt)
{
    //Python ptick event.
    NGF_PY_CALL_EVENT(ptick, evt.timeSinceLastFrame);
}
//-------------------------------------------------------------------------------
NGF::MessageReply Player::receiveMessage(NGF::Message msg)
{
    switch (msg.code)
    {
        case MSG_KEYPRESSED:
            if (GlbVar.paused || GlbVar.console->isVisible() || !mUnderControl)
                break;

            {
                OIS::KeyCode key = msg.getParam<OIS::KeyCode>(0);

                if (key == GlbVar.settings.controls.keys["dimensionSwitch"])
                    switchDimension();
                else if (key == GlbVar.settings.controls.keys["selfDestruct"])
                    die(true);
                else if (mCanSwitchGravity && key == GlbVar.settings.controls.keys["gravitySwitch"])
                {
                    mGravitySound->stop();
                    mGravitySound->play();
                    GlbVar.gravMgr->invert();
                }
            }
            NGF_NO_REPLY();

        case MSG_GETPOSITION:
            NGF_SEND_REPLY(BtOgre::Convert::toOgre(mBody->getWorldTransform().getOrigin()));

        case MSG_TELEPORT:
            {
                Ogre::Vector3 target = msg.getParam<Ogre::Vector3>(0);
                GlbVar.goMgr->sendMessage(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_TELEPORT, target));
                btTransform oldTrans = mBody->getWorldTransform();

                short int oldFlags = mBody->getBroadphaseHandle()->m_collisionFilterGroup;
                GlbVar.phyWorld->removeRigidBody(mBody);
                mBody->setWorldTransform(btTransform(oldTrans.getRotation(), BtOgre::Convert::toBullet(target)));
                GlbVar.phyWorld->addRigidBody(mBody, mDimensions | oldFlags, mDimensions);
            }
            NGF_NO_REPLY();

        case MSG_BULLETHIT:
            die(true);
            NGF_NO_REPLY();

        case MSG_CAPTURECAMERAHANDLER:
            captureCameraHandler();
            NGF_NO_REPLY();

        case MSG_WINLEVEL:
            mUnderControl = false;
            mWon = true;
            if (msg.getParam<bool>(0))
                mBody->setDamping(0.95, 0.95);
            NGF_SEND_REPLY();

        case MSG_ONEWAY:
            {
                //Get component in OneWay's 'bad' direction.
                btVector3 currVel = mBody->getLinearVelocity();
                btVector3 dir = -BtOgre::Convert::toBullet(msg.getParam<Ogre::Vector3>(0));
                Ogre::Real comp = dir.dot(currVel); //OneWay must send us normalised direction!

                //If we have a non-negative component in that direction, then we have to remove it, with some minimum removal to prevent
                //'pushing' through.
                if (comp >= 0)
                {
                    comp = comp > 0.5 ? comp : 0.5;
                    dir *= -comp;
                    mBody->setLinearVelocity(currVel + dir);
                }
            }

            NGF_NO_REPLY();

        case MSG_MAGNET:
            DO_MSG_MAGNET();

        case MSG_GETCRATEFORCE:
            btVector3 torque = mBody->getTotalTorque();
            NGF_SEND_REPLY(GlbVar.gravMgr->getSign() * Ogre::Vector3(-torque.z(), 0, torque.x()));
    }

    return GraLL2GameObject::receiveMessage(msg);
}
//-------------------------------------------------------------------------------
void Player::collide(GameObject *other, btCollisionObject *otherPhysicsObject, btManifoldPoint &contact)
{
    if (!other)
        return;

    //(Much) Less friction if not ground hit.
    Ogre::Vector3 hitPos = BtOgre::Convert::toOgre(contact.getPositionWorldOnA());
    if (GlbVar.gravMgr->getSign() * (mNode->getPosition().y - hitPos.y) < (mShape->getRadius() - 0.1))
        contact.m_combinedFriction = 0;

    //Do the GameObject type checks.
    if (other->hasFlag("Explosive"))
    {
        die(true);
        GlbVar.goMgr->sendMessage(other, NGF_MESSAGE(MSG_EXPLODE));
    }
    else if (other->hasFlag("Dangerous"))
    {
        die(false);
    }
    else if (other->hasFlag("Ice"))
    {
        contact.m_combinedFriction = GlbVar.goMgr->sendMessageWithReply<Ogre::Real>(other, NGF_MESSAGE(MSG_GETFRICTIONCOEFF));
    }
    else if (other->hasFlag("Pickup"))
    {
        Ogre::String pickupType = GlbVar.goMgr->sendMessageWithReply<Ogre::String>(other, NGF_MESSAGE(MSG_GETPICKUPTYPE));
        if (pickupType != "NONE")
        {
            ++mPickups[pickupType];
            GlbVar.goMgr->sendMessage(other, NGF_MESSAGE(MSG_PICKEDUP));
        }
    }
    
    //Python collide event.
    NGF::Python::PythonGameObject *oth = dynamic_cast<NGF::Python::PythonGameObject*>(other);
    if (oth)
        NGF_PY_CALL_EVENT(collide, oth->getConnector());
}
//-------------------------------------------------------------------------------

//--- Non-NGF -------------------------------------------------------------------
void Player::captureCameraHandler()
{
    //If it isn't already there we create it.
    if (!GlbVar.currCameraHandler)
    {
        GlbVar.currCameraHandler = GlbVar.goMgr->createObject<CameraHandler>(mControlNode->getPosition() + (mControlNode->getOrientation() * Ogre::Vector3(0,15,12)), mControlNode->getOrientation());
        GlbVar.goMgr->sendMessage(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_SETSMOOTHINGFACTOR, Ogre::Real(4)));
    }

    GlbVar.goMgr->sendMessage(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_SETTARGET, mControlNode));
    GlbVar.goMgr->sendMessage(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_SETCAMERASTATE, int(CameraHandler::CS_THIRDPERSON)));
}
//-------------------------------------------------------------------------------
void Player::loseCameraHandler()
{
    //If camera lookin' at us, tell him not to.
    if (GlbVar.currCameraHandler && (GlbVar.goMgr->sendMessageWithReply<Ogre::SceneNode*>(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_GETTARGET)) == mNode))
        GlbVar.goMgr->sendMessage(GlbVar.currCameraHandler, NGF_MESSAGE(MSG_SETCAMERASTATE, int(CameraHandler::CS_NONE)));
}
//-------------------------------------------------------------------------------
void Player::switchDimension()
{
    //Only if we can. :P
    if (!mCanSwitchDimensions)
        return;

    //Broadphase, narrowphase ghost check + ray even/odd test for trimeshes

    //First, do ghost test. Works for convex, and for trimesh-intersection.
    btManifoldArray manifoldArray;
    btBroadphasePairArray& pairArray = mGhostObject->getOverlappingPairCache()->getOverlappingPairArray();
    int numPairs = pairArray.size();

    for (int i = 0; i < numPairs; i++)
    {
        manifoldArray.clear();

        const btBroadphasePair& pair = pairArray[i];

        btBroadphasePair* collisionPair = GlbVar.phyWorld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
        if (!collisionPair)
            continue;

        if (collisionPair->m_algorithm)
            collisionPair->m_algorithm->getAllContactManifolds(manifoldArray);

        for (int j = 0; j < manifoldArray.size(); j++)
        {
            btPersistentManifold* manifold = manifoldArray[j];
            btCollisionObject *obj = (btCollisionObject*) ((manifold->getBody0() == mGhostObject) ? manifold->getBody1() : manifold->getBody0());

            if (obj == mBody) //It's us!
                break;

            short int flags = obj->getBroadphaseHandle()->m_collisionFilterGroup;
            if (flags & DimensionManager::NO_DIM_CHECK) //It doesn't want dimension test!
                break;

            for (int p = 0; p < manifold->getNumContacts(); p++)
            {
                const btManifoldPoint&pt = manifold->getContactPoint(p);

                if (pt.getDistance() < 0.f)
                {
                    if (flags & (mDimensions ^ DimensionManager::DIM_SWITCH)) //It's in the opposite dimension, abort!
                    {
                        mNoDimSound->stop(); 
                        mNoDimSound->play();
                        return;
                    }
                }
            }
        }
    }

    //Now, do trimesh-volume check. Use raycast, and then even-odd test.
    //http://en.wikipedia.org/wiki/Point_in_polygon
    struct PlayerTrimeshFind : public btDynamicsWorld::RayResultCallback
    {
        std::map<btCollisionObject*, bool> mHitMap;
        int mOppDimension;

        PlayerTrimeshFind(int oppDimension)
            : mOppDimension(oppDimension)
        {
        }

        btScalar addSingleResult(btDynamicsWorld::LocalRayResult &rayResult, bool)
        {
            btCollisionObject *obj = rayResult.m_collisionObject;
            mHitMap[obj] = !mHitMap[obj]; //Odd number of nots on 'false' (default bool constructor) is true, opposite for even.

            return 1; //Return 1 to get all hits.
        }

       bool needsCollision(btBroadphaseProxy* proxy0) const
        {
            //No non-dimension-check, only opposite dimension, only trimeshes (no need to ignore self, we're not trimesh anyway
            //so that test does that automatically).
            return (!(proxy0->m_collisionFilterGroup & DimensionManager::NO_DIM_CHECK))
                && (proxy0->m_collisionFilterGroup & mOppDimension) //Not really needed (we don't intersect with things in this dimension), but anyway.
                && ((btCollisionObject*)(proxy0->m_clientObject))->getCollisionShape()->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE;
        }
    } res(mDimensions ^ DimensionManager::DIM_SWITCH);

    btVector3 pos1 = mBody->getWorldTransform().getOrigin();
    btVector3 pos2 = pos1 + btVector3(0,100,0); //100 is big enough I guess. :P (cast upward)
    GlbVar.phyWorld->rayTest(pos1, pos2, res);

    for (std::map<btCollisionObject*, bool>::iterator iter = res.mHitMap.begin(); iter != res.mHitMap.end(); ++iter)
        if (iter->second) //The final bool for each trimesh is whether hits are odd (read previous comment).
        {
            mNoDimSound->stop(); 
            mNoDimSound->play();
            return;
        }

    //All clear, switch!
    GlbVar.dimMgr->switchDimension();
    setDimension(mDimensions ^ DimensionManager::DIM_SWITCH);
    mDimSound->stop();
    mDimSound->play();
}
//-------------------------------------------------------------------------------
void Player::die(bool explode, bool corpse, bool offLight)
{
    //We're not going through this twice!
    if (mDead)
        return;

    //If we're invincible, nah! :P
    if (mWon || mInvincible)
        return;

    //We lost the level.
    //if (!mWon)
    loseLevel();

    //Explosions!
    if (explode)
    {
        Util::createExplosion(mNode->getPosition());
        GlbVar.playerExplosionSound->stop();
        GlbVar.playerExplosionSound->play();
    }

    //And of course, we don't exist anymore. :-( If leaving a corpse, then just lose
    //control, switch of light if needed.
    if (corpse)
    {
        mUnderControl = false;

        if (offLight)
            lightOff();
    }
    else
        GlbVar.goMgr->requestDestroy(getID());
    mDead = true;
}
//-------------------------------------------------------------------------------

//--- Python interface implementation -------------------------------------------
NGF_PY_BEGIN_IMPL(Player)
{
    NGF_PY_METHOD_IMPL(getControlOrientation)
    {
        NGF_PY_RETURN(mControlNode->getOrientation());
    }
    NGF_PY_METHOD_IMPL(loseCameraHandler)
    {
        loseCameraHandler();
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(captureCameraHandler)
    {
        captureCameraHandler();
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(switchDimension)
    {
        switchDimension();
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(numPickup)
    {
        Ogre::String type = py::extract<Ogre::String>(args[0]);
        NGF_PY_RETURN(mPickups[type]);
    }
    NGF_PY_METHOD_IMPL(hasPickup)
    {
        Ogre::String type = py::extract<Ogre::String>(args[0]);
        NGF_PY_RETURN(mPickups[type] > 0);
    }
    NGF_PY_METHOD_IMPL(incPickup)
    {
        Ogre::String type = py::extract<Ogre::String>(args[0]);
        ++mPickups[type];
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(decPickup)
    {
        Ogre::String type = py::extract<Ogre::String>(args[0]);
        if (mPickups[type] > 0)
            --mPickups[type];
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(hasDecPickup)
    {
        //Decrease number, and return if we have (doors etc.).
        Ogre::String type = py::extract<Ogre::String>(args[0]);
        NGF_PY_RETURN(mPickups[type] > 0 && mPickups[type]--);
    }
    NGF_PY_METHOD_IMPL(die)
    {
        switch (py::len(args))
        {
            default:
            case 3:
                die(py::extract<bool>(args[0]), py::extract<bool>(args[1]), 
                        py::extract<bool>(args[2]));
                break;

            case 2:
                die(py::extract<bool>(args[0]), py::extract<bool>(args[1]));
                break;

            case 1:
                die(py::extract<bool>(args[0]));
                break;

            case 0:
                die();
                break;
        }

        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(lightOff)
    {
        lightOff();
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(lightOn)
    {
        lightOn();
        NGF_PY_RETURN();
    }
    NGF_PY_METHOD_IMPL(setGravitySwitchTime)
    {
        NGF_PY_METHOD_PARAMS_1(Ogre::Real, time);

        //If we're already counting down, we remove the old timer.
        if (mGravityTime >= 0 && mGravityHUDId != -1)
            GlbVar.hud->removeTimer(mGravityHUDId);

        mGravityTime = time;
        mGravityHUDId = GlbVar.hud->addTimer(time, Ogre::ColourValue(0.8, 0.6, 0.2));
        NGF_PY_RETURN();
    }

    NGF_PY_PROPERTY_IMPL(underControl, mUnderControl, bool)
    NGF_PY_PROPERTY_IMPL(invincible, mInvincible, bool)
    NGF_PY_PROPERTY_IMPL(canSwitchDimensions, mCanSwitchDimensions, bool)
    NGF_PY_PROPERTY_IMPL(canSwitchGravity, mCanSwitchGravity, bool)
}
NGF_PY_END_IMPL_BASE(GraLL2GameObject)
//-------------------------------------------------------------------------------
