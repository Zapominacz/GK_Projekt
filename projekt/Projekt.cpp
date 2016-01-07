#include <windows.h>
#include <cmath>
#include <GL/glut.h>
#include <GL/gl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

/* 
	Definicje typów 
*/
using Point3d = float[3];

struct Light
{
	Point3d position;
	Point3d specular;
	Point3d diffuse;
	Point3d ambient;
};

struct Sphere
{
	float radius;
	Point3d position;
	Point3d specular;
	Point3d diffuse;
	Point3d ambient;
	float specularhininess;
};

//Ustawienia sceny
struct Scene
{
	//Parametry swiatla rozproszonego
	Point3d globalAmbient;
	//kolor tla wczytywany z pliku
	Point3d backgroundColor;
	//Wielkoœæ obrazu
	int imageWidth, imageHeight;
	//Obiekty na scenie
	std::vector<Light> lights;
	std::vector<Sphere> spheres;
};
Scene scene;

//Opis promienia
struct Ray
{
	//Aktualny kolor promienia
	Point3d currentColor = { 0.0, 0.0 ,0.0 };
	//wspolrzedne (x,y,z) punktu przeciecia promienia i sfery
	Point3d lastIntersectionPoint;
	Point3d directionVector = {0.0, 0.0, -1.0};
	Light* currentLightSource;
	Sphere* currentSphere;
};

/* 
	Sta³e 
*/

//Plik sceny
const char* FILE_NAME = "scene_0.txt";

//Sta³e oœwietlenia
const float A = 1.0;
const float B = 0.01;
const float C = 0.001;

//Maksymalna odleg³oœæ któr¹ mo¿e pokonaæ 
//promieñ œwiat³a miedzy obiektami
const float MAX_DISTANCE = 1000;
//Ograniczenie iloœci iteracji
const unsigned ITERATION_LIMIT = 4;
//Wielkoœæ Ÿród³a œwiat³a
const float LIGHT_SIZE = 0;
//Rozmiar okna obserwatora
const float viewportSize = 15.0;

//Iloczyn skalarny
float inline scalarProduct(Point3d p1, Point3d p2)
{
	return p1[0] * p2[0] + p1[1] * p2[1] + p1[2] * p2[2];
}

//Odgleg³oœæ Euklidesowa
float inline distance(Point3d p1, Point3d p2)
{
	return sqrt(pow(p1[0] - p2[0], 2.0) + pow(p1[1] - p2[1], 2.0) + pow(p1[2] - p2[2], 2.0));
}

//Normalizacja wektora
void inline vectorNormalization(Point3d vector)
{
	auto d = sqrt(scalarProduct(vector, vector));
	if (d > 0.0)
	{
		vector[0] /= d;
		vector[1] /= d;
		vector[2] /= d;
	}
}


//Wyliczanie d³ugoœci promienia œwiat³a do danego obiektu
float computeLenght(Ray* ray, Point3d objectPosition, float objectSize)
{
	auto length = 0.0; //zerowa d³ugoœæ 
	auto a = scalarProduct(ray->directionVector, ray->directionVector);
	Point3d tmp;
	tmp[0] = ray->lastIntersectionPoint[0] - objectPosition[0];
	tmp[1] = ray->lastIntersectionPoint[1] - objectPosition[1];
	tmp[2] = ray->lastIntersectionPoint[2] - objectPosition[2];
	auto b = 2.0 * scalarProduct(ray->directionVector, tmp);
	auto c = scalarProduct(tmp, tmp) - objectSize;
	auto delta = b * b - 4.0 * a * c;
	if (delta >= 0)
	{
		length = (-b - sqrt(delta)) / (2.0 * a);
	}
	return length;
}


//Obliczanie punktu przeciêcia promienia z najbli¿szym obiektem
void intersection(Ray* ray)
{
	//Pocz¹tkowo z niczym siê nie przecina
	ray->currentSphere = nullptr;
	ray->currentLightSource = nullptr;
	auto length = MAX_DISTANCE;
	//sprawdzanie dla ka¿dego obiektu
	for (auto light = scene.lights.begin(); light != scene.lights.end(); ++light)
	{
		auto currentLength = computeLenght(ray, light->position, LIGHT_SIZE);
		if(currentLength > 0 && currentLength < length)
		{
			ray->currentLightSource = &(*light);
			length = currentLength;
		}
	}
	for (auto sphere = scene.spheres.begin(); sphere != scene.spheres.end(); ++sphere)
	{
		auto currentLength = computeLenght(ray, sphere->position, pow(sphere->radius, 2));
		if(currentLength > 0 && currentLength < length)
		{
			ray->currentLightSource = nullptr; //gdy obiekt jest wczeœniej ni¿ Ÿrod³o œwiat³a
			length = currentLength;
			ray->currentSphere = &(*sphere);
			ray->lastIntersectionPoint[0] += currentLength * ray->directionVector[0];
			ray->lastIntersectionPoint[1] += currentLength * ray->directionVector[1];
			ray->lastIntersectionPoint[2] += currentLength * ray->directionVector[2];
		}

	}
}

//Obliczenie kierunku promienia po odbiciu
void reflection(Point3d shpereNormalVector, Ray* ray)
{
	Point3d tmp = {
		-ray->directionVector[0],
		-ray->directionVector[1],
		-ray->directionVector[2]
	};
	auto nDotI = scalarProduct(tmp, shpereNormalVector);
	ray->directionVector[0] += 2 * nDotI * shpereNormalVector[0];
	ray->directionVector[1] += 2 * nDotI * shpereNormalVector[1];
	ray->directionVector[2] += 2 * nDotI * shpereNormalVector[2];
	vectorNormalization(ray->directionVector);
}

void computeSphereNormalVector(Point3d sphereNormalVector, Ray* ray)
{
	Sphere* sphere = ray->currentSphere;
	sphereNormalVector[0] = ray->lastIntersectionPoint[0] - sphere->position[0];
	sphereNormalVector[1] = ray->lastIntersectionPoint[1] - sphere->position[1];
	sphereNormalVector[2] = ray->lastIntersectionPoint[2] - sphere->position[2];
	vectorNormalization(sphereNormalVector);
}

//Obliczanie oœwietlenia zgodnie z modelem oœwietlenia Phonga
void phongLightModel(Ray* ray, Point3d shpereNormalVector)
{
	Sphere* sphere = ray->currentSphere;
	Point3d viewer = { 0, 0, 1.0f };
	for (auto light = scene.lights.begin(); light != scene.lights.end(); ++light)
	{
		Point3d lightVector;
		lightVector[0] = light->position[0] - ray->lastIntersectionPoint[0];
		lightVector[1] = light->position[1] - ray->lastIntersectionPoint[1];
		lightVector[2] = light->position[2] - ray->lastIntersectionPoint[2];
		vectorNormalization(lightVector);     

		auto nDotI = scalarProduct(lightVector, shpereNormalVector);
		Point3d reflectionVector;
		reflectionVector[0] = 2 * nDotI * shpereNormalVector[0] - lightVector[0];
		reflectionVector[1] = 2 * nDotI * shpereNormalVector[1] - lightVector[1];
		reflectionVector[2] = 2 * nDotI * shpereNormalVector[2] - lightVector[2];
		vectorNormalization(reflectionVector);

		auto vDotR = scalarProduct(reflectionVector, viewer);
		if (vDotR < 0) {
			vDotR = 0;
		}
		if (nDotI > 0)
		{
			auto lightModifier = distance(light->position, ray->lastIntersectionPoint);
			lightModifier = 1.0 / (A + B * lightModifier + C * pow(lightModifier, 2));
			auto specularhininess = pow(vDotR, sphere->specularhininess);
			ray->currentColor[0] += lightModifier * (sphere->diffuse[0] * light->diffuse[0] * nDotI + sphere->specular[0] * light->specular[0] * specularhininess) + sphere->ambient[0] * light->ambient[0];
			ray->currentColor[1] += lightModifier * (sphere->diffuse[1] * light->diffuse[1] * nDotI + sphere->specular[1] * light->specular[1] * specularhininess) + sphere->ambient[1] * light->ambient[1];
			ray->currentColor[2] += lightModifier * (sphere->diffuse[2] * light->diffuse[2] * nDotI + sphere->specular[2] * light->specular[2] * specularhininess) + sphere->ambient[2] * light->ambient[2];
		}
	}
	//Swiatlo rozproszone
	ray->currentColor[0] += sphere->ambient[0] * scene.globalAmbient[0];
	ray->currentColor[1] += sphere->ambient[1] * scene.globalAmbient[1];
	ray->currentColor[2] += sphere->ambient[2] * scene.globalAmbient[2];
}



//G³owna funkcja rekurencyjna opowiadaj¹ca za œledzenie promienia
void rayTrace(Ray* ray, unsigned iteration)
{
	intersection(ray); //badanie przeciêcia promienia
	if(ray->currentLightSource != nullptr)
	{ //jeœli przeci¹³ Ÿród³o œwiat³a, dodaj sk³adow¹ kierunkow¹ i zakoñcz rekurencjê
		float* specular = ray->currentLightSource->specular;
		ray->currentColor[0] += specular[0];
		ray->currentColor[1] += specular[1];
		ray->currentColor[2] += specular[2];
	} 
	else if (ray->currentSphere != nullptr)
	{ //gdy trafi³ obiekt oblicz i dodaj oœwietlenie, wylicz kierunek odbitego œwiat³a
		Point3d sphereNormal;
		computeSphereNormalVector(sphereNormal, ray);
		reflection(sphereNormal, ray);
		phongLightModel(ray, sphereNormal);
		++iteration; // i wykonaj nastêpn¹ iteracjê jeœli nie zosta³ osi¹gniêty limit
		if (iteration < ITERATION_LIMIT) {
			return rayTrace(ray, iteration);
		}
	}
}

void initScene(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-viewportSize / 2, viewportSize / 2, -viewportSize / 2, 
			viewportSize / 2, -viewportSize / 2, viewportSize / 2);
	glMatrixMode(GL_MODELVIEW);
}

//œledzenie promienia dla ka¿dego punktu na ekranie
void onRenderScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();
	for (auto col = scene.imageHeight / 2; col > -scene.imageHeight / 2; --col)
	{
		for (auto row = -scene.imageWidth / 2; row < scene.imageWidth / 2; ++row)
		{
			auto x = static_cast<float>(row) * viewportSize / scene.imageWidth;
			auto y = static_cast<float>(col) * viewportSize / scene.imageHeight;

			//tworzenie promienia i rozpoczêcie rekurencji
			Ray ray;
			ray.lastIntersectionPoint[0] = x;
			ray.lastIntersectionPoint[1] = y;
			ray.lastIntersectionPoint[2] = viewportSize;
			rayTrace(&ray, 0);

			GLubyte pixel[3] = {255, 255, 255};
			float* color = ray.currentColor;
			//Kolor inicjalizowany na pocz¹tku jest taki sam po operacjach - nic nie zosta³o przeciête
			//Ustawiamy kolor t³a
			if(color[0] <= 0 && color[1] <= 0 && color[2] <= 0)
			{
				color = scene.backgroundColor;
			}
			ray.currentColor[0] >= 1 ? pixel[0] = 255 : pixel[0] *= color[0];
			ray.currentColor[1] >= 1 ? pixel[1] = 255 : pixel[1] *= color[1];
			ray.currentColor[2] >= 1 ? pixel[2] = 255 : pixel[2] *= color[2];
			glRasterPos3f(x, y, 0);
			glDrawPixels(1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
		}
		glFlush();
	}
	
}

/*
	£aduje scenê z pliku
*/
void loadScene()
{
	using namespace std;
	fstream file(FILE_NAME, ios::in);
	if (!file.is_open())
	{
		cerr << "Nie znaleziono pliku!" << endl;
		cin.get();
		exit(0);
	}
	string buffer;
	file >> buffer;
	file >> scene.imageWidth;
	file >> scene.imageHeight;
	file >> buffer;
	file >> scene.backgroundColor[0];
	file >> scene.backgroundColor[1];
	file >> scene.backgroundColor[2];
	file >> buffer;
	file >> scene.globalAmbient[0];
	file >> scene.globalAmbient[1];
	file >> scene.globalAmbient[2];

	file >> buffer;
	while (buffer == "sphere" && !file.eof())
	{
		Sphere newSphere;
		file >> newSphere.radius;
		file >> newSphere.position[0];
		file >> newSphere.position[1];
		file >> newSphere.position[2];
		file >> newSphere.specular[0];
		file >> newSphere.specular[1];
		file >> newSphere.specular[2];
		file >> newSphere.diffuse[0];
		file >> newSphere.diffuse[1];
		file >> newSphere.diffuse[2];
		file >> newSphere.ambient[0];
		file >> newSphere.ambient[1];
		file >> newSphere.ambient[2];
		file >> newSphere.specularhininess;
		scene.spheres.push_back(newSphere);
		file >> buffer;
	}
	while (buffer == "source" && !file.eof())
	{
		Light light;
		file >> light.position[0];
		file >> light.position[1];
		file >> light.position[2];
		file >> light.specular[0];
		file >> light.specular[1];
		file >> light.specular[2];
		file >> light.diffuse[0];
		file >> light.diffuse[1];
		file >> light.diffuse[2];
		file >> light.ambient[0];
		file >> light.ambient[1];
		file >> light.ambient[2];
		scene.lights.push_back(light);
		file >> buffer;
	}
	file.close();
}

void main(void)
{
	loadScene();
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
	glutInitWindowSize(scene.imageWidth, scene.imageHeight);
	glutCreateWindow("209773 - projekt");
	initScene();
	glutDisplayFunc(onRenderScene);
	glutMainLoop();
}

