#include "Ogre.h"
#include "OgreApplicationContext.h"
#include "OgreInput.h"
#include "OgreRTShaderSystem.h"
#include <iostream>
#include <cstdio>

using namespace Ogre;
using namespace OgreBites;

const int lineCounter = 1000;

class LineDrawer
	: public ApplicationContext
	, public InputListener
{
	int pressCounter = 0;
	struct Point
	{
		double x, y, z;
		Point avgWith(const Point &b, double t) {
			return Point{t * x + (1 - t) * b.x, t * y + (1 - t) * b.y, t * z + (1 - t) * b.z};
		}
	}base[4], line[lineCounter];
	
	SceneManager* scnMgr;
	SceneNode* camNode;
	
	Plane backgoundPlane = Plane(0, 0, 1, 0);

public:
	LineDrawer(): ApplicationContext("LineDrawerApp") {}
	virtual ~LineDrawer() {}


	void setup()
	{
		// do not forget to call the base first
		ApplicationContext::setup();
		addInputListener(this);

		// get a pointer to the already created root
		Root* root = getRoot();
		scnMgr = root->createSceneManager();
		

		// register our scene with the RTSS
		RTShader::ShaderGenerator* shadergen = RTShader::ShaderGenerator::getSingletonPtr();
		shadergen->addSceneManager(scnMgr);

		//! [turnlights]
		scnMgr->setAmbientLight(ColourValue(0.5, 0.5, 0.5));

		//! [newlight]
		Light* light = scnMgr->createLight("MainLight");
		SceneNode* lightNode = scnMgr->getRootSceneNode()->createChildSceneNode();
		lightNode->attachObject(light);


		//! [lightpos]
		lightNode->setPosition(20, 80, 50);

		//! [camera]
		camNode = scnMgr->getRootSceneNode()->createChildSceneNode();

		// create the camera
		Camera* cam = scnMgr->createCamera("myCam");
		cam->setNearClipDistance(5); // specific to this sample
		cam->setAutoAspectRatio(true);
		camNode->attachObject(cam);
		camNode->setPosition(0, 0, 140);

		// and tell it to render into the main window
		getRenderWindow()->addViewport(cam);
		//! [camera]

		/*std::cout << scnMgr->getCamera("myCam")->getViewport()->getActualWidth() << std::endl;
		std::cout << scnMgr->getCamera("myCam")->getViewport()->getActualHeight() << std::endl;*/
		
	}

	//bool mousePressed(const OIS::MouseEvent& me, OIS::MouseButtonID id) {
	bool mousePressed(const MouseButtonEvent& e) {
		Camera* viewCamera = scnMgr->getCamera("myCam");

		if ((e.button & BUTTON_LEFT) && pressCounter < 4)
		{
			// Setup the ray scene query
			double x = 1.0 * e.x / viewCamera->getViewport()->getActualWidth();
			double y = 1.0 * e.y / viewCamera->getViewport()->getActualHeight();
			//printf("---%lf, %lf----", x, y);
			Ray mouseRay = viewCamera->getCameraToViewportRay(x, y);
			RayTestResult testRes = mouseRay.intersects(backgoundPlane);
			Vector3 res = mouseRay.getPoint(testRes.second);
			base[pressCounter++] = Point{ res[0], res[1], res[2] };
			//printf("%s %.2lf, %.2lf, %.2lf\n", testRes.first ? "Ture" : "False", res[0], res[1], res[2]);
			if (pressCounter == 4)
				draw();
			
		} 
		return false;
	}

	void draw() {
		Ogre::ManualObject* myManual = scnMgr->createManualObject("manual_line");
		Ogre::SceneNode* myManualNode = scnMgr->getRootSceneNode()->createChildSceneNode("manualLineNode");
		
		myManual->begin("manual1Material", Ogre::RenderOperation::OT_LINE_STRIP);
		for (int i = 0; i < lineCounter; i++) {
			double t = 1.0 * i / lineCounter;
			Point ab = base[0].avgWith(base[1], t), bc = base[1].avgWith(base[2], t), cd = base[2].avgWith(base[3], t);
			Point abc = ab.avgWith(bc, t), bcd = bc.avgWith(cd, t);
			Point res = abc.avgWith(bcd, t);
			//printf("%.2lf, %.2lf, %.2lf\n", res.x, res.y, res.z);
			myManual->position(res.x, res.y, res.z);
			myManual->position(res.x, res.y, res.z);
		}
		myManual->end();
		
		myManualNode->attachObject(myManual);
	}

};


int main(int argc, char **argv)
{
	try
	{
		LineDrawer app;
		app.initApp();
		app.getRoot()->startRendering();
		app.closeApp();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error occurred during execution: " << e.what() << '\n';
		return 1;
	}

	return 0;
}