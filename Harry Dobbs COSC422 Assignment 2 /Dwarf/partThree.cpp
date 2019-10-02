//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: ModelLoader.cpp
//  
//  Press key '1' to toggle 90 degs model rotation about x-axis on/off.
//  ========================================================================

#include <iostream>
#include <map>
#include <GL/freeglut.h>
#include <IL/il.h>
#include <vector>
using namespace std;

#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assimp_extras.h"

//----------Globals----------------------------
const aiScene* scene = NULL;
const aiScene* animation = NULL;

double angle = 0;
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = false;
std::map<int, int> texIdMap;
int tDuration;
int currTick = 0;
float timeStep = 50;
aiVector3D offset = aiVector3D(1.0f,1.0f,1.0f);
double player_x = 10;
double player_z = 0;
double zoomfactor = 1;
double floorMoveSpeed = 0.00001;
int animationMode = 0;

// Mesh Struture to hold initial values
struct meshInit
{
	int mNumVertices;
	aiVector3D* mVertices;
	aiVector3D* mNormals;

};

meshInit* initData;



//------------Modify the following as needed----------------------
float materialCol[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   //Default material colour (not used if model's colour is available)
bool replaceCol = false;					   //Change to 'true' to set the model's colour to the above colour
bool twoSidedLight = false;					   //Change to 'true' to enable two-sided lighting

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char* fileName,int fileNumber)
{
	if(fileNumber == 1)
	{
		scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality| aiProcess_Debone);
	}
	if(fileNumber == 2)
	{
		animation = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
	}
	
	if(scene == NULL) exit(1);
	//printSceneInfo(scene);
	//printMeshInfo(scene);
	//printTreeInfo(scene->mRootNode);
	//printBoneInfo(scene);
	printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data
	printAnimInfo(animation);  //WARNING:  This may generate a lengthy output if the model has animation data
	get_bounding_box(scene, &scene_min, &scene_max);
	
	return true;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene* scene)
{
	/* initialization of DevIL */
	ilInit();
	if (scene->HasTextures())
	{
		std::cout << "Support for meshes with embedded textures is not implemented" << endl;
		return;
	}

	/* scan scene's materials for textures */
	/* Simplified version: Retrieves only the first texture with index 0 if present*/
	for (unsigned int m = 0; m < scene->mNumMaterials; ++m)
	{
		aiString path;  // filename
		if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
		{
			glEnable(GL_TEXTURE_2D);
			ILuint imageId;
			GLuint texId;
			ilGenImages(1, &imageId);
			glGenTextures(1, &texId);
			texIdMap[m] = texId;   //store tex ID against material id in a hash map
			ilBindImage(imageId); /* Binding of DevIL image name */
			ilEnable(IL_ORIGIN_SET);
			ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
			

			
			if (ilLoadImage((ILstring)path.data))  
			{
				/* Convert image to RGBA */
				ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

				/* Create and load textures to OpenGL */
				glBindTexture(GL_TEXTURE_2D, texId);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
					ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
					ilGetData());
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				cout << "Texture:" << path.data << " successfully loaded." << endl;
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
				glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
			}
			else
			{
				cout << "Couldn't load Image: " << path.data << endl;
			}
		}
	}  //loop for material

}

// ------A recursive function to traverse scene graph and render each mesh----------
void render (const aiScene* sc, const aiNode* nd)
{
	aiMatrix4x4 m = nd->mTransformation;
	aiMesh* mesh;
	aiFace* face;
	aiMaterial* mtl;
	aiColor4D diffuse;
	int meshIndex, materialIndex;

	aiTransposeMatrix4(&m);   //Convert to column-major order
	glPushMatrix();
	glMultMatrixf((float*)&m);   //Multiply by the transformation matrix for this node

	// Draw all meshes assigned to this node
	for (uint n = 0; n < nd->mNumMeshes; n++)
	{
		meshIndex = nd->mMeshes[n];          //Get the mesh indices from the current node
		mesh = scene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

		materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh
		mtl = sc->mMaterials[materialIndex];
	
	    if (mesh->HasTextureCoords(0))
	    {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D,texIdMap[meshIndex]);
			
			
		}
		
		if (replaceCol)
			glColor4fv(materialCol);   //User-defined colour
		else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))  //Get material colour from model
			glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
		else
			glColor4fv(materialCol);   //Default material colour

		//Get the polygons from each mesh and draw them
		for (uint k = 0; k < mesh->mNumFaces; k++)
		{
			face = &mesh->mFaces[k];
			GLenum face_mode;

			switch(face->mNumIndices)
			{
				case 1: face_mode = GL_POINTS; break;
				case 2: face_mode = GL_LINES; break;
				case 3: face_mode = GL_TRIANGLES; break;
				default: face_mode = GL_POLYGON; break;
			}

			glBegin(face_mode);
			for(uint i = 0; i < face->mNumIndices; i++) {
				int vertexIndex = face->mIndices[i]; 
				if(mesh->HasVertexColors(0))
					glColor4fv((GLfloat*)&mesh->mColors[0][vertexIndex]);

				//Assign texture coordinates here

				if (mesh->HasNormals())
					glNormal3fv(&mesh->mNormals[vertexIndex].x);
				glVertex3fv(&mesh->mVertices[vertexIndex].x);

				
				if (mesh->HasTextureCoords(0))
				{
					glTexCoord2f(mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y);
				}
				
			}
			glEnd();
		}
	}

	// Draw all children
	for (uint i = 0; i < nd->mNumChildren; i++)
		render(sc, nd->mChildren[i]);
	glPopMatrix();
}

//--------------------OpenGL initialization------------------------
void initialise()
{
	float ambient[4] = { 0.8, 0.8, 0.8, 1.0 };  //Ambient light
	float white[4] = { 1, 1, 1, 1 };			//Light's colour
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white);
	if (twoSidedLight) glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
	glColor4fv(materialCol);
	loadModel("dwarf.x",1);	
	loadModel("avatar_walk.bvh",2);	

	//avatar_walk.bvh		//<<<-------------Specify input file name here
	loadGLTextures(scene);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(35, 1, 1.0, 1000.0);
		
    initData = new meshInit[scene->mNumMeshes];

	// Storing Inital Data of Vertices into structure
	for (uint i = 0; i < scene->mNumMeshes; i++)
    {
	
		aiMesh *mesh = scene->mMeshes[i];
	
		(initData + i)->mNumVertices = mesh->mNumVertices;
	    (initData + i)->mVertices = new aiVector3D[mesh->mNumVertices];
		(initData + i)->mNormals = new aiVector3D[mesh->mNumVertices];	
		
		for(uint x = 0; x < mesh->mNumVertices; x++)
		{
			(initData + i)->mVertices[x] = mesh->mVertices[x];
			(initData + i)->mNormals[x] = mesh->mNormals[x];
			
		}		
	}
	tDuration = scene->mAnimations[0]->mDuration;
}



void updateModel(double tick)
{
	int index;
	aiAnimation* anim = scene->mAnimations[0];
	aiAnimation* anim2 = animation->mAnimations[0];

	aiMatrix4x4 matPos, matRot, matProd;
	aiMatrix3x3 matRot3;
	aiNode* nd;
	aiMatrix4x4 boneTransform;
	aiNodeAnim* ndAnim ;
	// Step 1 Get the Transformation Matrix for each channel and replace the joint's
	// transformation matrix using function from LAB
	
	
	//aiNodeAnim* rKnee = anim2->mChannels[0]; //Channel

	
	for (uint i = 0; i < anim->mNumChannels; i++)
	{

		
		matPos = aiMatrix4x4(); //Identity
		matRot = aiMatrix4x4();
		ndAnim = anim->mChannels[i]; //Channel

		
		// Position Interpolation
		aiVector3D posn;
		aiVector3D nextPosn;
		aiVector3D prevPosn;
		double prevTime;
		double nextTime;
		if(i != 1 or animationMode != 1)
		{
			for(uint posFrame = 0; posFrame < ndAnim->mNumPositionKeys; posFrame ++)
			{
			
				if(tick >= ndAnim->mPositionKeys[posFrame].mTime)
				{
					prevPosn = ndAnim->mPositionKeys[posFrame].mValue;
					prevTime = ndAnim->mPositionKeys[posFrame].mTime;
					continue;
					
				}
				nextPosn = ndAnim->mPositionKeys[posFrame].mValue;
				nextTime = ndAnim->mPositionKeys[posFrame].mTime;
				double timeFactor = (tick - prevTime)/(nextTime - prevTime) ;
				posn = nextPosn + float(timeFactor) * (prevPosn - nextPosn);
				matPos.Translation(posn, matPos);
				break;
					
			}
		}
					if(ndAnim->mNumPositionKeys == 1)
			{
				posn = ndAnim->mPositionKeys[0].mValue;
				matPos.Translation(posn, matPos);
			}
		if(animationMode == 1)
		{

			
			if(i == 1)// middle
			{
				ndAnim = anim2->mChannels[1];//Channel
				tick = 0.2364*tick;	
			}
			
			if(i == 2)// lefthips
			{
				ndAnim = anim2->mChannels[15];//Channel
				tick = 0.2364*tick;	
			}
			if(i == 3) //lknee
			{
				ndAnim = anim2->mChannels[16];//Channel
				tick = 0.2364*tick;	
			}
			if(i == 4)// lankle
			{
				ndAnim = anim2->mChannels[17];//Channel
				tick = 0.2364*tick;	
			}
			if(i == 6) //rthigh
			{
				ndAnim = anim2->mChannels[18];//Channel
				tick = 0.2364*tick;	
			}		
			if(i == 7) //rknee
			{
				ndAnim = anim2->mChannels[19];//Channel
				tick = 0.2364*tick;	
			}
			if(i == 8) //rankle
			{
				ndAnim = anim2->mChannels[20];//Channel
				tick = 0.2364*tick;	
			}
		}
		
		// Quaternion Interpolation
		aiQuaternion rotn;
		aiQuaternion prevRot;
		aiQuaternion nextRot;
		double rotPrevTime;
		double rotNextTime;
		for(uint rotFrame = 0; ndAnim->mNumRotationKeys;rotFrame++)
		{
			if(tick >= ndAnim->mRotationKeys[rotFrame].mTime)
			{
				prevRot = ndAnim->mRotationKeys[rotFrame].mValue;
				rotPrevTime = ndAnim->mRotationKeys[rotFrame].mTime;
				continue;
			}
			
			nextRot = ndAnim->mRotationKeys[rotFrame].mValue;
			rotNextTime = ndAnim->mRotationKeys[rotFrame].mTime;
			double timeFactor2 = (tick - rotPrevTime)/(rotNextTime - rotPrevTime);
			rotn.Interpolate(rotn,prevRot,nextRot,timeFactor2);
			/*
			if(rotFrame == 1)
			{
				rotn = ndAnim2->mRotationKeys[rotFrame].mValue;

			}
			*/
			matRot3 = rotn.GetMatrix();
			matRot = aiMatrix4x4(matRot3);
			break;
			
		}
	
		if(ndAnim->mNumRotationKeys == 1)
		{
			rotn = ndAnim->mRotationKeys[0].mValue;
			matRot3 = rotn.GetMatrix();
			matRot = aiMatrix4x4(matRot3);
		}
		
		// change back to other channel animation
				
		if((i== 1 or i == 2 or i == 3 or i == 4 or i ==6 or i == 7 or i == 8) && animationMode == 1)
		{
			ndAnim = anim->mChannels[i]; //Channe
			tick =(1/ 0.2364)*tick;	

		}
		
		matProd = matPos * matRot;
		nd = scene->mRootNode->FindNode(ndAnim->mNodeName);
		nd->mTransformation = matProd;
	}
	
	for (uint meshID = 0; meshID < scene->mNumMeshes; meshID ++)
	{
		
		aiMesh *mesh = scene->mMeshes[meshID];	
		for (uint i = 0; i < mesh->mNumBones; i ++)
		{

			aiBone *bone = mesh->mBones[i];	
		    aiNode *node = scene->mRootNode->FindNode(bone->mName);

			// Step 2 Use the Offset Matrices of bones to transform mesh vertices to node space
			aiMatrix4x4 boneOffsetMatrix = bone->mOffsetMatrix;
			boneTransform = boneOffsetMatrix;
			
			// Step 3 get node parent transformations and use them to create the transformation matrix
			
			while (node != nullptr)
            {
				boneTransform = node->mTransformation * boneTransform;
				node = node->mParent;
            }
            
            // Transpose Matrix for Normal Transformation
            
            aiMatrix4x4 boneTransformTransposeInverse = boneTransform;
            boneTransformTransposeInverse.Transpose().Inverse();
            
            
            // Applying the transformations to each vertex
            
            for(uint k = 0; k < bone->mNumWeights; k++)
            {
				unsigned int vid = (bone->mWeights[k]).mVertexId;
				// get Inital data
				aiVector3D vert = (initData + meshID)->mVertices[vid];
				aiVector3D norm = (initData + meshID)->mNormals[vid];
				
				aiMatrix3x3 aiVert3x3 = aiMatrix3x3(boneTransform);
				aiMatrix3x3 aiNorm3x3 = aiMatrix3x3(boneTransformTransposeInverse);
				// preform transformations on inital data and update the mesh.
				mesh->mVertices[vid] =  ((aiVert3x3 * vert) + aiVector3D(boneTransform.a4 , boneTransform.b4 , boneTransform.c4)) ;
				mesh->mNormals[vid] = ((aiNorm3x3 * norm) + aiVector3D(boneTransformTransposeInverse.a4 , boneTransformTransposeInverse.b4,boneTransformTransposeInverse.c4));
			}				
		}
	}
}


//----Timer callback for continuous rotation of the model about y-axis----
void update(int value)
{
	
	if(currTick < tDuration)
	{
		updateModel(currTick);
		currTick ++;
	}
	if(currTick >= tDuration)
	{
		currTick = 0;
	}
	glutTimerFunc(timeStep,update,0);

	glutPostRedisplay();
	
	/*
	angle++;
	if(angle > 360) angle = 0;
	glutPostRedisplay();
	glutTimerFunc(50, update, 0);
	*/
}

//----Keyboard callback to toggle initial model orientation---
void keyboard(unsigned char key, int x, int y)
{
	if(key == '1') animationMode = 0;   //Enable/disable initial model rotation
	if(key == '2') animationMode = 1;   
	glutPostRedisplay();
}



void drawFloorPlane()
{
    float white[4] = {0., 1., 1.,1.0};
    glDisable(GL_TEXTURE_2D);

    glColor4f(0.0, 0.72, 0.56, 1.0);  //The floor is gray in colour
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    //The floor is made up of several tiny squares on a 200x200 grid. Each square has a unit size.

    glBegin(GL_QUADS);
    glNormal3f(0.0, .0, -1.0);
    floorMoveSpeed += 0.85 * animationMode;
	
	for(int i = 0 ; i < 500 ; i++)
	{
	
	if(i % 2 == 0)
	{
		    glColor4f(1.0, 0.2, 0.56, 1.0);  //The floor is gray in colour

	}
	else{    glColor4f(0.0, 0.72, 0.56, 1.0); }
	
    glVertex3f(-1000 + (i * 200) - floorMoveSpeed,2000 ,31);
    glVertex3f(-1000 +  (i * 200) - floorMoveSpeed,-2000 ,31);
    glVertex3f(-800 + (i * 200) - floorMoveSpeed,-2000 ,31);
    glVertex3f(-800  + (i * 200) - floorMoveSpeed, 2000,31);
            
	}

    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glEnd();

}

 void special(int key, int x, int y)
 {
	  if(key == GLUT_KEY_LEFT)
		{
			angle+= 5;
		}
		else if(key == GLUT_KEY_RIGHT)
		{
			angle-= 5;
		}
		else if(key == GLUT_KEY_DOWN)
		{
			if (zoomfactor < 1)
			{zoomfactor += 0.05;
		glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(35.0* zoomfactor, 1.0, 1.0, 1000.0 );}

		}
		else if(key == GLUT_KEY_UP)
		{
			if (zoomfactor > 0.3)
			{zoomfactor -= 0.05;

		glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(35.0 * zoomfactor, 1.0, 1.0, 1000.0 );}
		}
		//std::cout << "Zoom Factor" << zoomfactor << endl;
}




//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_LIGHTING);
	
	  // glDisable ( GL_LIGHTING ) ;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	float lightPosn[4] = { 50, 50, 50, 1 };         //Default light's position

	gluLookAt(0,0, 15, 0, 0, 0, 0, 1, 0);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);
	
	glRotatef(angle, 0, 1, 0);

	//glRotatef(90, 1, 0, 0);
   // glRotatef(90, 0, 0, 1);

	//glRotatef(angle, 0.f, 1.f ,0.f);  //Continuous rotation about the y-axis
	//if(modelRotn) glRotatef(-90, 1, 0, 0);		  //First, rotate the model about x-axis if needed.

	// scale the whole asset to fit into our view frustum 
	float tmp = scene_max.x - scene_min.x;
	tmp = aisgl_max(scene_max.y - scene_min.y,tmp);
	tmp = aisgl_max(scene_max.z - scene_min.z,tmp);
	tmp = 1.f / tmp;
	glScalef(tmp, tmp, tmp);

	float xc = (scene_min.x + scene_max.x)*0.5;
	float yc = (scene_min.y + scene_max.y)*0.5;
	float zc = (scene_min.z + scene_max.z)*0.5;
	// center the model
	glPushMatrix();
		glTranslatef(0,(animationMode * 28) + 5 , 0);
		glTranslatef(-xc, -yc, -zc);
		render(scene, scene->mRootNode);
	glPopMatrix();
    
    glPushMatrix();
		glRotatef(-90, 0, 1,0 );
		glRotatef(90, 1, 0, 0);
		    drawFloorPlane();

	glPopMatrix();

	glutSwapBuffers();
}



int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(600, 600);
	glutCreateWindow("Model Loader");
	glutInitContextVersion (4, 2);
	glutInitContextProfile ( GLUT_CORE_PROFILE );

	initialise();
	glutDisplayFunc(display);
	glutTimerFunc(timeStep, update, 0);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutMainLoop();

	aiReleaseImport(scene);
}

