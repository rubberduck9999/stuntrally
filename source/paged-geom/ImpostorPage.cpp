/*-------------------------------------------------------------------------------------
Copyright (c) 2006 John Judnich
Modified 2008 by Erik Hjortsberg (erik.hjortsberg@iteam.se)

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------------*/

//ImpostorPage.cpp
//ImposterPage is an extension to PagedGeometry which displays entities as imposters.
//-------------------------------------------------------------------------------------
#include "pch.h"
#include <Ogre.h>
// #include <OgreRoot.h>
// #include <OgreTimer.h>
// #include <OgreCamera.h>
// #include <OgreVector3.h>
// #include <OgreQuaternion.h>
// #include <OgreEntity.h>
// #include <OgreSubEntity.h>
// #include <OgreHardwarePixelBuffer.h>
// #include <OgreViewport.h>

#include "ImpostorPage.h"
#include "StaticBillboardSet.h"

#include "../ogre/common/Def_Str.h"
#include "../vdrift/pathmanager.h"

#include "../shiny/Main/Factory.hpp"
#include "../shiny/Platforms/Ogre/OgreMaterial.hpp"

namespace Forests {

using namespace Ogre;
using namespace Forests;

// static members initialization
Ogre::uint  Forests::ImpostorPage::s_nImpostorResolution    = 256;
Ogre::uint  Forests::ImpostorPage::s_nSelfInstances         = 0;
Ogre::uint  Forests::ImpostorPage::s_nUpdateInstanceID      = 0;
ColourValue Forests::ImpostorPage::s_clrImpostorBackground  = ColourValue(0.0f, 0.3f, 0.0f, 0.0f);
BillboardOrigin Forests::ImpostorPage::s_impostorPivot      = BBO_CENTER;



//-----------------------------------------------------------------------------
/// Default constructor
ImpostorPage::ImpostorPage() :
m_pSceneMgr    (NULL),
	m_pPagedGeom   (NULL),
	m_blendMode    (ALPHA_REJECT_IMPOSTOR),
	m_nInstanceID  (0),
	m_nAveCount    (0),
	m_vecCenter    (0, 0, 0)
{
	++s_nSelfInstances;
}


//-----------------------------------------------------------------------------
/// Destructor
ImpostorPage::~ImpostorPage()
{
	TImpostorBatchs::iterator iter = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (iter != iend)
	{
		delete iter->second;
		++iter;
	}

	if (--s_nSelfInstances == 0 && m_pPagedGeom)
	{
		if (m_pPagedGeom->getSceneNode())
		{
			m_pPagedGeom->getSceneNode()->removeAndDestroyChild("ImpostorPage::renderNode");
			m_pPagedGeom->getSceneNode()->removeAndDestroyChild("ImpostorPage::cameraNode");
		}
		else if (m_pSceneMgr)
		{
			m_pSceneMgr->destroySceneNode("ImpostorPage::renderNode");
			m_pSceneMgr->destroySceneNode("ImpostorPage::cameraNode");
		}
		else
		{
			assert(false && "Who must delete scene node???");
		}

		try
		{	ResourceGroupManager::getSingleton().destroyResourceGroup("BinFolder");  //Impostors
		}
		catch (Exception&)
		{
			LogO("## destroyResourceGroup Impostors ~dtor fail");
		}
	}
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::init(PagedGeometry *geom, const Ogre::Any &data)
{
	assert(geom && "Null pointer to PagedGeometry");
	m_pSceneMgr    = geom->getSceneManager();
	m_pPagedGeom   = geom;
	// LogO("# ImpostorPage::init");

	if (s_nSelfInstances == 1)  // first instance
	{
		// Set up a single instance of a scene node which will be used when rendering impostor textures
		geom->getSceneNode()->createChildSceneNode("ImpostorPage::renderNode");
		geom->getSceneNode()->createChildSceneNode("ImpostorPage::cameraNode");
		LogO("# createResourceGroup BinFolder");
		ResourceGroupManager::getSingleton().createResourceGroup("BinFolder");  //Impostors
		LogO("# createResourceGroup BinFolder ok");
	}
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::setRegion(Ogre::Real left, Ogre::Real top, Ogre::Real right, Ogre::Real bottom)
{
	// Calculate center of region
	m_vecCenter.x  = (left + right) * 0.5f;
	m_vecCenter.z  = (top + bottom) * 0.5f;
	m_vecCenter.y  = 0.0f; // The center.y value is calculated when the entities are added
	m_nAveCount    = 0;
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::addEntity(Entity *ent, const Vector3 &position, const Quaternion &rotation, const Vector3 &scale, const Ogre::ColourValue &color)
{
	//Get the impostor batch that this impostor will be added to
	ImpostorBatch *ibatch = ImpostorBatch::getBatch(this, ent);

	//Then add the impostor to the batch
	ibatch->addBillboard(position, rotation, scale, color);

	//Add the Y position to the center.y value (to be averaged later)
	m_vecCenter.y += position.y + ent->getBoundingBox().getCenter().y * scale.y;
	++m_nAveCount;
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::build()
{
	if (m_mapImpostorBatches.empty())
		return;

	// Calculate the average Y value of all the added entities
	m_vecCenter.y = m_nAveCount > 0 ? m_vecCenter.y / m_nAveCount : 0;

	//Build all batches
	TImpostorBatchs::iterator it = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (it != iend)
	{
		it->second->build();
		++it;
	}
}

//-----------------------------------------------------------------------------
///
void ImpostorPage::setVisible(bool visible)
{
	// Update visibility status of all batches
	TImpostorBatchs::iterator it = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (it != iend)
	{
		it->second->setVisible(visible);
		++it;
	}
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::setFade(bool enabled, Real visibleDist, Real invisibleDist)
{
	// Update fade status of all batches
	TImpostorBatchs::iterator it = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (it != iend)
	{
		it->second->setFade(enabled, visibleDist, invisibleDist);
		++it;
	}
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::removeEntities()
{
	TImpostorBatchs::iterator iter = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (iter != iend)
	{
		iter->second->clear();
		++iter;
	}

	//Reset y center
	m_vecCenter.y  = 0;
	m_nAveCount    = 0;
}


//-----------------------------------------------------------------------------
///
void ImpostorPage::update()
{
	if (m_mapImpostorBatches.empty())  // SVA speed up
		return;

	//Calculate the direction the impostor batches should be facing
	Vector3 camPos = m_pPagedGeom->_convertToLocal(m_pPagedGeom->getCamera()->getDerivedPosition());

	// Update all batches
	Ogre::Real distX = camPos.x - m_vecCenter.x;
	Ogre::Real distZ = camPos.z - m_vecCenter.z;
	Ogre::Real distY = camPos.y - m_vecCenter.y;
	Ogre::Real distRelZ = Math::Sqrt(distX * distX + distZ * distZ);
	Radian pitch = Math::ATan2(distY, distRelZ);

	Radian yaw;
	if (distRelZ > m_pPagedGeom->getPageSize() * 3)
	{
		yaw = Math::ATan2(distX, distZ);
	}
	else
	{
		Vector3 dir = m_pPagedGeom->_convertToLocal(m_pPagedGeom->getCamera()->getDerivedDirection());
		yaw = Math::ATan2(-dir.x, -dir.z);
	}

	TImpostorBatchs::iterator iter = m_mapImpostorBatches.begin(), iend = m_mapImpostorBatches.end();
	while (iter != iend)
	{
		iter->second->setAngle(pitch.valueDegrees(), yaw.valueDegrees());
		++iter;
	}
}

void ImpostorPage::regenerate(Entity *ent)
{
	ImpostorTexture *tex = ImpostorTexture::getTexture(NULL, ent);
	if (tex != NULL)
		tex->regenerate();
}

void ImpostorPage::regenerateAll()
{
	ImpostorTexture::regenerateAll();
}


//-------------------------------------------------------------------------------------

unsigned long ImpostorBatch::GUID = 0;

ImpostorBatch::ImpostorBatch(ImpostorPage *group, Entity *entity) :
m_pTexture  (NULL)
{
	// Render impostor texture for this entity
	m_pTexture = ImpostorTexture::getTexture(group, entity);

	//Create billboard set
	PagedGeometry *pg = group->getParentPagedGeometry();
	bbset = new StaticBillboardSet(pg->getSceneManager(), pg->getSceneNode());
	bbset->setTextureStacksAndSlices(IMPOSTOR_PITCH_ANGLES, IMPOSTOR_YAW_ANGLES);

	setBillboardOrigin(ImpostorPage::getImpostorPivot());

	//Default the angle to 0 degrees
	pitchIndex = -1;
	yawIndex = -1;
	setAngle(0.0f, 0.0f);

	//Init. variables
	igroup = group;
}

ImpostorBatch::~ImpostorBatch()
{
	//Delete billboard set
	delete bbset;

	// Delete texture
	ImpostorTexture::removeTexture(m_pTexture);
}

//Returns a pointer to an ImpostorBatch for the specified entity in the specified
//ImpostorPage. If one does not already exist, one will automatically be created.
ImpostorBatch *ImpostorBatch::getBatch(ImpostorPage *group, Entity *entity)
{
	//Search for an existing impostor batch for this entity
	String entityKey = ImpostorBatch::generateEntityKey(entity);
	ImpostorBatch *batch = group->getImpostorBatch(entityKey);
	if (batch)  // If found return it
		return batch;

	// Otherwise, create a new batch
	batch = new ImpostorBatch(group, entity);
	group->injectImpostorBatch(entityKey, batch);   // warning! function can return false
	return batch;
}

//Rotates all the impostors to the specified angle (virtually - it actually changes
//their materials to produce this same effect)
void ImpostorBatch::setAngle(Ogre::Real pitchDeg, Ogre::Real yawDeg)
{
	// Calculate pitch material index
	int newPitchIndex = 0;
#ifdef IMPOSTOR_RENDER_ABOVE_ONLY
	if (pitchDeg > 0)
	{
		float maxPitchIndexDeg = (90.0f * (IMPOSTOR_PITCH_ANGLES-1)) / IMPOSTOR_PITCH_ANGLES;
		newPitchIndex = (int)(IMPOSTOR_PITCH_ANGLES * (pitchDeg / maxPitchIndexDeg));
		if (newPitchIndex > IMPOSTOR_PITCH_ANGLES-1) newPitchIndex = IMPOSTOR_PITCH_ANGLES-1;
	}
#else
	float minPitchIndexDeg = -90.0f;
	float maxPitchIndexDeg = ((180.0f * (IMPOSTOR_PITCH_ANGLES-1)) / IMPOSTOR_PITCH_ANGLES) - 90.0f;
	newPitchIndex = (int)(IMPOSTOR_PITCH_ANGLES * ((pitchDeg - minPitchIndexDeg) / (maxPitchIndexDeg - minPitchIndexDeg)));
	if (newPitchIndex > IMPOSTOR_PITCH_ANGLES-1) newPitchIndex = IMPOSTOR_PITCH_ANGLES-1;
	if (newPitchIndex < 0) newPitchIndex = 0;
#endif

	// Calculate yaw material index
	int newYawIndex = yawDeg > 0 ? int(IMPOSTOR_YAW_ANGLES * (yawDeg / 360.0f) + 0.5f) % IMPOSTOR_YAW_ANGLES :
		int(IMPOSTOR_YAW_ANGLES + IMPOSTOR_YAW_ANGLES * (yawDeg / 360.0f) + 0.5f) % IMPOSTOR_YAW_ANGLES;

	// Change materials if necessary
	if (newPitchIndex != pitchIndex || newYawIndex != yawIndex)
	{
		pitchIndex = newPitchIndex;
		yawIndex = newYawIndex;
		bbset->setMaterial(m_pTexture->material[pitchIndex][yawIndex]->getName());
	}
}

void ImpostorBatch::setBillboardOrigin(BillboardOrigin origin)
{
	bbset->setBillboardOrigin(origin);

	if (bbset->getBillboardOrigin() == BBO_CENTER)
		entityBBCenter = m_pTexture->entityCenter;
	else if (bbset->getBillboardOrigin() == BBO_BOTTOM_CENTER)
		entityBBCenter = Vector3(m_pTexture->entityCenter.x, m_pTexture->entityCenter.y - m_pTexture->entityRadius, m_pTexture->entityCenter.z);
}

String ImpostorBatch::generateEntityKey(Entity *entity)
{
	return entity->getMesh()->getName();  ///T + easier (AND avoids crash when having material name with / in it)

	/*StringUtil::StrStreamType entityKey;
	entityKey << entity->getMesh()->getName();
	for (unsigned int i = 0; i < entity->getNumSubEntities(); ++i)
	{
	entityKey << "-" << entity->getSubEntity(i)->getMaterialName();
	}
	entityKey << "-" << IMPOSTOR_YAW_ANGLES << "_" << IMPOSTOR_PITCH_ANGLES;
	#ifdef IMPOSTOR_RENDER_ABOVE_ONLY
	entityKey << "_RAO";
	#endif
	return entityKey.str();*/
}

//-------------------------------------------------------------------------------------


ImpostorTextureResourceLoader::ImpostorTextureResourceLoader(ImpostorTexture& impostorTexture)
	: texture(impostorTexture)
{
}

void ImpostorTextureResourceLoader::loadResource (Ogre::Resource *resource)
{
	if (resource->getLoadingState() == Ogre::Resource::LOADSTATE_UNLOADED) {
		texture.regenerate();
	}
}

//-------------------------------------------------------------------------------------


std::map<String, ImpostorTexture *> ImpostorTexture::selfList;
unsigned long ImpostorTexture::GUID = 0;

//Do not use this constructor yourself - instead, call getTexture()
//to get/create an ImpostorTexture for an Entity.
ImpostorTexture::ImpostorTexture(ImpostorPage *group, Entity *entity, bool onlyToRender) :
	bOnlyToRender(onlyToRender)
{
	//Store scene manager and entity
	ImpostorTexture::sceneMgr = group->getParentPagedGeometry()->getSceneManager();
	ImpostorTexture::entity = entity;
	ImpostorTexture::group = group;

	//Add self to list of ImpostorTexture's
	entityKey = ImpostorBatch::generateEntityKey(entity);
	typedef std::pair<String, ImpostorTexture *> ListItem;
	selfList.insert(ListItem(entityKey, this));

	//Calculate the entity's bounding box and it's diameter
	boundingBox = entity->getBoundingBox();

	///T
	//entityRadius = Math::boundingRadiusFromAABB(boundingBox);
	Real tmp;
	entityRadius = boundingBox.getMaximum().x - boundingBox.getCenter().x;
	tmp = boundingBox.getMaximum().y - boundingBox.getCenter().y;	if (tmp > entityRadius) entityRadius = tmp;
	tmp = boundingBox.getMaximum().z - boundingBox.getCenter().z;	if (tmp > entityRadius) entityRadius = tmp;

	entityDiameter = 2.0f * entityRadius;
	entityCenter = boundingBox.getCenter();

	//Render impostor textures
	renderTextures(false);

	//Set up materials
	if (!bOnlyToRender)
	for (int o = 0; o < IMPOSTOR_YAW_ANGLES; ++o)
	for (int i = 0; i < IMPOSTOR_PITCH_ANGLES; ++i)
	{
		std::string matId = getUniqueID("ImpostorMaterial");
		sh::MaterialInstance* m = sh::Factory::getInstance().createMaterialInstance(matId, "ImposterBase");
		m->setProperty("diffuseMap", sh::makeProperty(texture->getName()));

		material[i][o] = MaterialManager::getSingleton().getByName(matId);

		float uscroll = (float)o / IMPOSTOR_YAW_ANGLES;
		float vscroll = (float)i / IMPOSTOR_PITCH_ANGLES;

		m->setProperty("scroll", sh::makeProperty(Ogre::StringConverter::toString(uscroll) + " " + Ogre::StringConverter::toString(vscroll)));

		//m->setProperty("scene_blend", sh::makeProperty<sh::StringValue>(new sh::StringValue("add")));
		//todo:^?
		if (group->getBlendMode() == ALPHA_REJECT_IMPOSTOR) {
			m->setProperty("alpha_rejection", sh::makeProperty("greater_equal " + Ogre::StringConverter::toString(128)));
			//p->setAlphaRejectSettings(CMPF_GREATER_EQUAL, 64);
		} else if (group->getBlendMode() == ALPHA_BLEND_IMPOSTOR) {
			m->setProperty("scene_blend", sh::makeProperty<sh::StringValue>(new sh::StringValue("src_alpha one_minus_src_alpha")));
			m->setProperty("depth_write", sh::makeProperty<sh::StringValue>(new sh::StringValue("off")));
		}
		sh::Factory::getInstance()._ensureMaterial(matId, "Default");
	}
}

void ImpostorTexture::updateMaterials()
{
	for (int o = 0; o < IMPOSTOR_YAW_ANGLES; ++o)
	for (int i = 0; i < IMPOSTOR_PITCH_ANGLES; ++i)
	{	Material *m = material[i][o].getPointer();

		sh::Factory::getInstance().getMaterialInstance(m->getName())->setProperty("diffuseMap", sh::makeProperty(texture->getName()));
		sh::Factory::getInstance()._ensureMaterial(m->getName(), "Default");
	}
}

ImpostorTexture::~ImpostorTexture()
{
	LogO("# ~ImpostorTexture()");
	if (!bOnlyToRender)
	{
		//Delete materials
		for (int o = 0; o < IMPOSTOR_YAW_ANGLES; ++o)
		for (int i = 0; i < IMPOSTOR_PITCH_ANGLES; ++i)
		{
			assert(material[i][o]);
			String matName(material[i][o]->getName());
			sh::Factory::getInstance().destroyMaterialInstance(matName);

			//material[i][o].reset();
			//if (MaterialManager::getSingletonPtr())
			//	MaterialManager::getSingleton().remove(matName);
		}

		//Delete textures
		assert(texture);
		String texName(texture->getName());
		if (TextureManager::getSingletonPtr())
		{
			TextureManager::getSingleton().remove(texture);
			//TextureManager::getSingleton().unload(texName);
		}
		texture.reset();
	}

	//Remove self from list of ImpostorTexture's
	selfList.erase(entityKey);
}

void ImpostorTexture::regenerate()
{
	assert(texture);
	//String texName(texture->getName());
	if (TextureManager::getSingletonPtr())
		TextureManager::getSingleton().remove(texture);
	texture.reset();
	renderTextures(true);
	updateMaterials();
}

void ImpostorTexture::regenerateAll()
{
	std::map<String, ImpostorTexture *>::iterator iter;
	for (iter = selfList.begin(); iter != selfList.end(); ++iter){
		iter->second->regenerate();
	}
}

void ImpostorTexture::renderTextures(bool force)
{
	Ogre::Timer ti;
#ifdef IMPOSTOR_FILE_SAVE
	TexturePtr renderTexture;
#else
	TexturePtr renderTexture(texture);
	//if we're not using a file image we need to set up a resource loader, so that the texture is regenerated if it's ever unloaded (such as switching between fullscreen and the desktop in win32)
	loader = std::make_unique<ImpostorTextureResourceLoader>(*this);
#endif
	RenderTexture *renderTarget;
	Camera *renderCamera;
	Viewport *renderViewport;
	SceneNode *camNode;

	//Set up RTT texture
	Ogre::uint textureSize = ImpostorPage::getImpostorResolution();
	if (!renderTexture)
	{
		renderTexture = TextureManager::getSingleton().createManual(getUniqueID("ImpostorTexture"), "BinFolder", //Impostors
			TEX_TYPE_2D, textureSize * IMPOSTOR_YAW_ANGLES, textureSize * IMPOSTOR_PITCH_ANGLES, 0, PF_A8R8G8B8, TU_RENDERTARGET, loader.get());
	}
	renderTexture->setNumMipmaps(255/*MIP_UNLIMITED*/);

	//Set up render target
	renderTarget = renderTexture->getBuffer()->getRenderTarget(); 
	renderTarget->setAutoUpdated(false);

	//Set up camera
	camNode = sceneMgr->getSceneNode("ImpostorPage::cameraNode");
	renderCamera = sceneMgr->createCamera(getUniqueID("ImpostorCam"));
	camNode->attachObject(renderCamera);
	renderCamera->setLodBias(1000.0f);
	renderViewport = renderTarget->addViewport(renderCamera);
	renderViewport->setOverlaysEnabled(false);
	renderViewport->setClearEveryFrame(true);
	renderViewport->setShadowsEnabled(false);
	renderViewport->setBackgroundColour(ColourValue(0,0,0,0));  //ImpostorPage::getImpostorBackgroundColor());
	renderViewport->setMaterialScheme ("impostor_rtt");

	//Set up scene node
	SceneNode* node = sceneMgr->getSceneNode("ImpostorPage::renderNode");

	Ogre::SceneNode* oldSceneNode = entity->getParentSceneNode();
	if (oldSceneNode) {
		oldSceneNode->detachObject(entity);
	}

	///T
	Ogre::SceneNode *n1= node->createChildSceneNode();
	n1->attachObject(entity);
	n1->setPosition(-entityCenter /*+ Vector3(10,0,10)*/);

	/*Entity *e2 = entity->clone(entity->getName() + "_clone");
	Ogre::SceneNode *n2= node->createChildSceneNode();
	n2->attachObject(e2);
	n2->setPosition(-entityCenter + Vector3(10,0,10));*/

	//Set up camera FOV
	const Real objDist = entityRadius * 100;
	const Real nearDist = objDist - (entityRadius + 1); 
	const Real farDist = objDist + (entityRadius + 1);

	renderCamera->setAspectRatio(1.0f);
	renderCamera->setFOVy(Math::ATan(entityDiameter / objDist));
	renderCamera->setNearClipDistance(/*nearDist*/ 0.1f); ///T
	renderCamera->setFarClipDistance(farDist);

	//Disable mipmapping (without this, masked textures look bad)
	MaterialManager *mm = MaterialManager::getSingletonPtr();
	FilterOptions oldMinFilter = mm->getDefaultTextureFiltering(FT_MIN);
	FilterOptions oldMagFilter = mm->getDefaultTextureFiltering(FT_MAG);
	FilterOptions oldMipFilter = mm->getDefaultTextureFiltering(FT_MIP);
	mm->setDefaultTextureFiltering(FO_POINT, FO_LINEAR, FO_NONE);

	//Disable fog
	FogMode oldFogMode = sceneMgr->getFogMode();
	ColourValue oldFogColor = sceneMgr->getFogColour();
	Real oldFogDensity = sceneMgr->getFogDensity();
	Real oldFogStart = sceneMgr->getFogStart();
	Real oldFogEnd = sceneMgr->getFogEnd();
	sceneMgr->setFog(FOG_NONE);


	// Get current status of the queue mode
	Ogre::SceneManager::SpecialCaseRenderQueueMode OldSpecialCaseRenderQueueMode = sceneMgr->getSpecialCaseRenderQueueMode();
	//Only render the entity
	sceneMgr->setSpecialCaseRenderQueueMode(Ogre::SceneManager::SCRQM_INCLUDE); 
	sceneMgr->addSpecialCaseRenderQueue(group->getParentPagedGeometry()->getRenderQueue() + 1);

	uint8 oldRenderQueueGroup = entity->getRenderQueueGroup();
	entity->setRenderQueueGroup(group->getParentPagedGeometry()->getRenderQueue() + 1);
	//e2->setRenderQueueGroup(entity->getRenderQueueGroup()); ///T
	bool oldVisible = entity->getVisible();
	entity->setVisible(true);
	//e2->setVisible(true); ///T
	Ogre::Real oldMaxDistance = entity->getRenderingDistance();
	entity->setRenderingDistance(0);
	//e2->setRenderingDistance(0); ///T

	bool needsRegen = true;
#ifdef IMPOSTOR_FILE_SAVE
	//Calculate the filename hash used to uniquely identity this render
	String strKey = entityKey;

	///T
	String tempdir = this->group->getParentPagedGeometry()->getTempDir();
	ResourceGroupManager::getSingleton().addResourceLocation(tempdir, "FileSystem", "BinFolder");

	String fileNamePNG = strKey + ".png";  //only

	//Attempt to load the pre-render file if allowed
	bool pre = false;
	needsRegen = force || group->getParentPagedGeometry()->forceRegenImpostors;  ///T
	if (!needsRegen)
	{
		bool has = ResourceGroupManager::getSingleton().resourceExists("BinFolder", fileNamePNG);
		if (has)
		try	// get preloaded
		{	texture = TextureManager::getSingleton().getByName(fileNamePNG, "BinFolder");
			pre = true;
		}catch (Ogre::Exception&)
		{	has = false;	}

		if (!has)  // load
		{	try{
				texture = TextureManager::getSingleton().load(fileNamePNG, "BinFolder", TEX_TYPE_2D, MIP_UNLIMITED);
			}catch (Ogre::Exception&)
			{	needsRegen = true;	}  // create
		}
	}
#endif
	static int ii = 0;  ///T

	if (needsRegen)
	{	//If this has not been pre-rendered, do so now
		const float xDivFactor = 1.0f / IMPOSTOR_YAW_ANGLES;
		const float yDivFactor = 1.0f / IMPOSTOR_PITCH_ANGLES;
		for (int o = 0; o < IMPOSTOR_PITCH_ANGLES; ++o)
		{	// 4 pitch angle renders
			#ifdef IMPOSTOR_RENDER_ABOVE_ONLY
			Radian pitch = Degree((90.0f * o) * yDivFactor); //0, 22.5, 45, 67.5
			#else
			Radian pitch = Degree((180.0f * o) * yDivFactor - 90.0f);
			#endif

			for (int i = 0; i < IMPOSTOR_YAW_ANGLES; ++i)
			{	// 4 yaw angle renders
				Radian yaw = Degree((360.0f * i) * xDivFactor); //0, 45, 90, 135, 180, 225, 270, 315

				//Position camera
				camNode->setPosition(0, 0, 0);
				camNode->setOrientation(Quaternion(yaw, Vector3::UNIT_Y) * Quaternion(-pitch, Vector3::UNIT_X));
				camNode->translate(Vector3(0, 0, objDist), Node::TS_LOCAL);

				//Render the impostor
				renderViewport->setDimensions((float)(i) * xDivFactor, (float)(o) * yDivFactor, xDivFactor, yDivFactor);
				renderTarget->update();
				if (ii == 0) {  ///T  render twice, 1st generated impostor missing on directx
					renderTarget->update();  ++ii;  }
			}
		}

#ifdef IMPOSTOR_FILE_SAVE
		//Save RTT to file with respecting the temp dir
		renderTarget->writeContentsToFile(tempdir + "/" + fileNamePNG);  ///T missing /

		//Load the render into the appropriate texture view
		try{
			texture = TextureManager::getSingleton().load(fileNamePNG, "BinFolder", TEX_TYPE_2D, MIP_UNLIMITED);
		}catch (Ogre::Exception&)
		{	LogO("## impostor failed load after png save: " + fileNamePNG);  }
#else
		texture = renderTexture;
#endif
	}


	entity->setVisible(oldVisible);
	entity->setRenderQueueGroup(oldRenderQueueGroup);
	entity->setRenderingDistance(oldMaxDistance);

	//sceneMgr->destroyEntity(e2); ///T

	sceneMgr->removeSpecialCaseRenderQueue(group->getParentPagedGeometry()->getRenderQueue() + 1);
	// Restore original state
	sceneMgr->setSpecialCaseRenderQueueMode(OldSpecialCaseRenderQueueMode); 

	//Re-enable mipmapping
	mm->setDefaultTextureFiltering(oldMinFilter, oldMagFilter, oldMipFilter);

	//Re-enable fog
	sceneMgr->setFog(oldFogMode, oldFogColor, oldFogDensity, oldFogStart, oldFogEnd);

	//Delete camera
	renderTarget->removeViewport(0);
	renderCamera->getSceneManager()->destroyCamera(renderCamera);

	//Delete scene node
	node->detachAllObjects();
	//n2->detachAllObjects(); ///T
	n1->detachAllObjects();
	node->removeAndDestroyAllChildren();
	if (oldSceneNode) {
		oldSceneNode->attachObject(entity);
	}

#ifdef IMPOSTOR_FILE_SAVE
	//Delete RTT texture
	assert(renderTexture);
	String texName2(renderTexture->getName());

	renderTexture.reset();
	if (TextureManager::getSingletonPtr())
	if (TextureManager::getSingleton().resourceExists(texName2))
		TextureManager::getSingleton().remove(texName2);
#endif

	Ogre::LogManager::getSingleton().logMessage(String("::: Time Impostor: ") +
		fToStr(ti.getMilliseconds(),0,3) + " ms (" + strKey + ") " + (needsRegen ? " Generated" : (pre ? " preloaded" : " loaded")));
}

String ImpostorTexture::removeInvalidCharacters(String s)
{
	std::stringstream s2;

	for (uint32 i = 0; i < s.length(); ++i){
		char c = s[i];
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|'){
			s2 << '-';
		} else {
			s2 << c;
		}
	}

	return s2.str();
}

void ImpostorTexture::removeTexture(ImpostorTexture* Texture)
{
	//Search for an existing impostor texture, in case it was already deleted
	for(std::map<String, ImpostorTexture *>::iterator iter=selfList.begin();
		iter!=selfList.end(); ++iter)
	{
		if(iter->second==Texture)
		{
			delete Texture;
			return;
		}
	}
	// no need to anything if it was not found, chances are that it was already deleted
}

ImpostorTexture *ImpostorTexture::getTexture(ImpostorPage *group, Entity *entity)
{
	//Search for an existing impostor texture for the given entity
	String entityKey = ImpostorBatch::generateEntityKey(entity);
	std::map<String, ImpostorTexture *>::iterator iter;
	iter = selfList.find(entityKey);

	//If found..
	if (iter != selfList.end()){
		//Return it
		return iter->second;		
	} else {
		if (group){
			//Otherwise, return a new texture
			return (new ImpostorTexture(group, entity));
		} else {
			//But if group is null, return null
			return NULL;
		}
	}
}

} // namespace Forests
