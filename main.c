/* 
 * File:   main.c
 * Author: David
 *
 * Created on October 2, 2016, 10:45 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

//Structures
typedef struct {
  int kind; // 0 = cylinder, 1 = sphere
  double color[3];
  double center[3];
  union {
    struct {
      double normal[3];
      double difuse_color[3];
      double specular_color[3];      
    } plane;
    struct {
      double radius;
      double difuse_color[3];
      double specular_color[3];
    } sphere;
    struct {
      double height;
      double width;
    } camera;
    struct{
        double radial2;
        double radial1;
        double radial0;
        double angular0;
        double direction[3];
        double theta;
    } light;
  };
} Object;
typedef struct{
    char r;
    char g;
    char b;
    } Pixel;
    
    //Global Variables
    double h = 0.7;
    double w = 0.7;
    int line = 1;
    Pixel* image;
    Object** objects;
    Object** lights;
    #define MAXCOLOR 255 
    void set_camera(FILE* json);
    Object camera;
    //sqr function
static inline double sqr(double v) {
  return v*v;
}

//normalize function
static inline void normalize(double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

//checks for sphere intersection
double sphere_intersection(double* Ro, double* Rd,
			     double* C, double r) {

  double a = sqr(Rd[0]) + sqr(Rd[2]) + sqr(Rd[1]);
  double b = (2 * (Rd[0] * (Ro[0] - C[0]) + Rd[2] * (Ro[2] - C[2]) + Rd[1] * (Ro[1] - C[1])));
  double c = sqr(Ro[0]-C[0]) + sqr(Ro[2]-C[2]) + sqr(Ro[1]-C[1]) - sqr(r);

  double det = sqr(b) - 4 * a * c;
  if (det < 0) 
      return -1;

  det = sqrt(det);
  
  double t0 = (-b - det) / (2*a);
  if (t0 > 0) 
      return t0;

  double t1 = (-b + det) / (2*a);
  if (t1 > 0) 
      return t1;

  return -1;
}

//checks for plane intersection
double plane_intersection(double* Ro, double* Rd, double* C, double* normal){
    double d = normal[0]*C[0] + normal[1]*C[1] + normal[2]*C[2];
    double t = -(normal[0]*Ro[0] + normal[1]*Ro[1] + normal[2]*Ro[2] + d)/(normal[0]*Rd[0] + normal[1]*Rd[1] + normal[2]*Rd[2]);
    if (t > 0){
        return t;
    }else{
        return -1;
    }
}

// next_c() wraps the getc() function and provides error checking and line
// number maintenance
int next_c(FILE* json) {
  int c = fgetc(json);

  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}


char* next_string(FILE* json) {
  char buffer[129];
  int c = next_c(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }  
  c = next_c(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);      
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);      
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }
  buffer[i] = 0;
  return strdup(buffer);
}

// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d) {
  int c = next_c(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);    
}


// skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
  int c = next_c(json);
  while (isspace(c)) {
    c = next_c(json);
  }
  ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.


double next_number(FILE* json) {
  double value;
  if (fscanf(json, "%lf", &value) != 1){
      fprintf(stderr,"Number value not found on line %d.", line);
      return -1;
  }
  return value;
}

double* next_vector(FILE* json) {
  double* v = malloc(3*sizeof(double));
  expect_c(json, '[');
  skip_ws(json);
  v[0] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[1] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[2] = next_number(json);
  skip_ws(json);
  expect_c(json, ']');
  return v;
}

//begins the parsing of the file
void read_scene(char* filename) {
  int c;
  FILE* json = fopen(filename, "r");
  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }
  
  skip_ws(json);
  
  // Find the beginning of the list
  expect_c(json, '[');
  skip_ws(json);

  // Find the objects
  int i = 0;
  int j = 0;
  while (1) {
    c = fgetc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      return;
    }
    if (c == '{') {
        objects[i] = malloc(sizeof(Object));
        lights[i] = malloc(sizeof(Object));
      skip_ws(json);
    
      // Parse the object
      char* key = next_string(json);
      if (strcmp(key, "type") != 0) {
	fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
	exit(1);
      }

      skip_ws(json);

      expect_c(json, ':');

      skip_ws(json);

      char* value = next_string(json);
      
      //Sets value of the object or arranges the camera
      if (strcmp(value, "camera") == 0) {
          set_camera(json);
      } else if (strcmp(value, "sphere") == 0) {
          
          objects[i]->kind = 1;
          
      } else if (strcmp(value, "plane") == 0) {
          
          objects[i]->kind = 0;
          
      }else if(strcmp(value, "light") == 0){
  
          lights[j]->kind = 2;
          
      } else {
	fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
	exit(1);
      }

      skip_ws(json);
      //Makes sure we skip the object if it is the camera because of the function set_camera
      if (strcmp(value, "camera") != 0){
        while (1) {
          // , }
          c = next_c(json);

          if (c == '}') {
            // stop parsing this object
            break;
          } else if (c == ',') {
              
            // read another field
            skip_ws(json);
            char* key = next_string(json);
            skip_ws(json);
            expect_c(json, ':');
            skip_ws(json);
            
            //sets radius
            if (strcmp(key, "radius") == 0) {
              double value = next_number(json);
               if(objects[i]->kind == 1){
                  objects[i]->sphere.radius = value;
              }else{
                   fprintf(stderr, "Radius should only to attached to a sphere.");
              }
              //Checks for position,color, and normal fields
            } else if ((strcmp(key, "color") == 0) ||
                       (strcmp(key, "position") == 0) ||
                       (strcmp(key, "normal") == 0) ||
                       (strcmp(key, "diffuse_color") == 0)||
                       (strcmp(key, "specular_color") == 0)||
                       (strcmp(key, "direction") == 0)) {
              double* value = next_vector(json);
              
              //sets position and color for sphere
              if(objects[i]->kind == 1){
                  if(strcmp(key, "position") == 0){
                      objects[i]->center[0] = value[0];
                      objects[i]->center[1] = -value[1];
                      objects[i]->center[2] = value[2];
                  
                  }else if(strcmp(key, "color") == 0){
                      objects[i]->color[0] = value[0];
                      objects[i]->color[1] = value[1];
                      objects[i]->color[2] = value[2];
                  }else if(strcmp(key, "diffuse_color") == 0){
                      objects[i]->sphere.difuse_color[0] = value[0];
                      objects[i]->sphere.difuse_color[1] = value[1];
                      objects[i]->sphere.difuse_color[2] = value[2];
                  }else if(strcmp(key, "specular_color") == 0){
                      objects[i]->sphere.specular_color[0] = value[0];
                      objects[i]->sphere.specular_color[1] = value[1];
                      objects[i]->sphere.specular_color[2] = value[2];
                  }else{
                      fprintf(stderr, "Non-valid field entered for a sphere");
                      exit(1);
                 }
                  //sets position and color for plane
              }else if(objects[i]->kind == 0){
                  if(strcmp(key, "position") == 0){
                      objects[i]->center[0] = value[0];
                      objects[i]->center[1] = value[1];
                      objects[i]->center[2] = value[2];
                  }else if(strcmp(key, "color") == 0){
                      objects[i]->color[0] = value[0];
                      objects[i]->color[1] = value[1];
                      objects[i]->color[2] = value[2];
                  }else if(strcmp(key,"normal") == 0) {
                      objects[i]->plane.normal[0] = value[0];
                      objects[i]->plane.normal[1] = value[1];
                      objects[i]->plane.normal[2] = value[2];
                  }else if(strcmp(key, "diffuse_color") == 0){
                      objects[i]->plane.difuse_color[0] = value[0];
                      objects[i]->plane.difuse_color[1] = value[1];
                      objects[i]->plane.difuse_color[2] = value[2];
                  }else if(strcmp(key, "specular_color") == 0){
                      objects[i]->plane.specular_color[0] = value[0];
                      objects[i]->plane.specular_color[1] = value[1];
                      objects[i]->plane.specular_color[2] = value[2];
                  }else{
                      fprintf(stderr, "Non-valid field entered for a plane");
                      exit(1);
                 }
              }else if(lights[j]->kind == 2){
                  if(strcmp(key, "position") == 0){
                      lights[j]->center[0] = value[0];
                      lights[j]->center[1] = value[1];
                      lights[j]->center[2] = value[2];
                  }else if(strcmp(key, "color") == 0){
                      lights[j]->color[0] = value[0];
                      lights[j]->color[1] = value[1];
                      lights[j]->color[2] = value[2];
                  }else if(strcmp(key,"direction") == 0) {
                      lights[j]->light.direction[0] = value[0];
                      lights[j]->light.direction[1] = value[1];
                      lights[j]->light.direction[2] = value[2];
                  }else{
                      fprintf(stderr, "Non-valid field entered for a plane");
                      exit(1);
                 }
              }

            } else if ((strcmp(key, "radial-a2") == 0) ||
                       (strcmp(key, "radial-a1") == 0) ||
                       (strcmp(key, "radial-a0") == 0) ||
                       (strcmp(key, "angular-a0") == 0) ||
                       (strcmp(key, "theta") == 0)) {
                double value = next_number(json);
                  if(lights[j]->kind == 2){
                      if(strcmp(key, "radial-a2") == 0){
                          lights[j]->light.radial2 = value;
                      }else if (strcmp(key, "radial-a1") == 0){
                          lights[j]->light.radial1 = value;
                      }else if (strcmp(key, "radial-a0") == 0){
                          lights[j]->light.radial0 = value;
                      }else if (strcmp(key, "angular-a0") == 0){
                          lights[j]->light.angular0 = value;
                      }else if(strcmp(key,"theta") == 0) {
                          lights[j]->light.theta = value;
                      }
                  }     
            } else {
              fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                      key, line);
              //char* value = next_string(json);
            }
            skip_ws(json);
          } else {
            fprintf(stderr, "Error: Unexpected value on line %d\n", line);
            exit(1);
          }
        }
      i++;
      j++;
      }
      
      skip_ws(json);
      c = next_c(json);
      if (c == ',') {
	// noop
	skip_ws(json);
      } else if (c == ']') {
          objects[i] = NULL;
          lights[i] = NULL;
	fclose(json);
	return;
      } else {
	fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
	exit(1);
      }
    }
  }
  
}

double static clamp(double s){
 if(s > 1){
     return 1;
 }else if (s < 0){
     return s;
 }else{
     return s;
 }
}

int main(int argc, char** argv) {
    objects = malloc(sizeof(Object*)*129);
    lights = malloc(sizeof(Object*)*129);
    int index = 0;
    FILE* outputfile;
    //checks for number of arguments
    if(argc != 5){
        fprintf(stderr, "Please put the commands in the following format: height, weight, source file, destination file.");
        exit(1);
    }
    read_scene(argv[3]);
    Object* object;
  
  double cx = camera.center[0];
  double cy = camera.center[1];
  double cz = camera.center[2];
//grabs height and width of pixel
  int M = atoi(argv[2]);
  int N = atoi(argv[1]);
  if(M <= 0 || N <= 0){
      fprintf(stderr, "Please make Height and Width a positive integer.");
      exit(1);
  }
  image = malloc(sizeof(Pixel)*M*N);
  double pixheight = camera.camera.height / M;
  double pixwidth = camera.camera.width / N;
  
  //Set the objects into the proper place and set the image pixels
  for (int y = 0; y < M; y += 1) {
      for (int x = 0; x < N; x += 1) {
        double Ro[3] = {cx, cy, cz};
        // Rd = normalize(P - Ro)
        double Rd[3] = {
          cx - (camera.camera.width/2) + pixwidth * (x + 0.5),
          cy - (camera.camera.height/2) + pixheight * (y + 0.5),
          1
        };

        normalize(Rd);
        double best_t = INFINITY;
        Object* object2;
        for (int i=0; objects[i] != 0; i += 1) {
            double t = 0;
            switch(objects[i]->kind) {
              case 0:
                  t = plane_intersection(Ro, Rd,objects[i]->center, objects[i]->plane.normal);
                  break;

              case 1:
                  t = sphere_intersection(Ro, Rd,objects[i]->center, objects[i]->sphere.radius);
                  break;

              default:
            // Horrible error
                  exit(1);
          }
          
          if (t > 0 && t < best_t){
              best_t = t;
              object2 = object;
              object = malloc(sizeof(Object));
              memcpy(object, objects[i], sizeof(Object));
          } 
        }
            //set the color for the pixel
        if (best_t > 0 && best_t != INFINITY) {
            double color[3];
            color[0] = 0;
            color[1] = 0;
            color[2] = 0;
            double object_position[3];
            double Pixel_position[3];
            double N[3];
            Pixel_position[0] = Rd[0] * best_t + Ro[0];
            object_position[0] = camera.center[0] - Pixel_position[0];
            Pixel_position[1] = Rd[1] * best_t + Ro[1];
            object_position[1] = camera.center[1] - Pixel_position[1];
            Pixel_position[2] = Rd[2] * best_t + Ro[2];
            object_position[2] = camera.center[2] - Pixel_position[2];
            normalize(object_position);
            if (object->kind == 1){
                N[0] = Pixel_position[0] - object2->center[0];
                N[1] = Pixel_position[1] - object2->center[1];
                N[2] = Pixel_position[2] - object2->center[2];
            }else if (object->kind == 2){
                N[0] = object2->plane.normal[0];
                N[1] = object2->plane.normal[1];
                N[2] = object2->plane.normal[2];
            }
            normalize(N);
            for(int j = 0; lights[j] != NULL; j += 1){
                double light_object[3];
                double object_light[3];
                object_light[0] = lights[j]->center[0] - Pixel_position[0];
                object_light[1] = lights[j]->center[1] - Pixel_position[1];
                object_light[2] = lights[j]->center[2] - Pixel_position[2];
                light_object[0] = Rd[0] * best_t + Ro[0];
                light_object[1] = Rd[1] * best_t + Ro[1];
                light_object[2] = Rd[2] * best_t + Ro[2];
                normalize(light_object);
                normalize(object_light);
                double shadow = 0;
                double dl = sqrt(sqr(Pixel_position[0] - lights[j]->center[0])
                + sqr(Pixel_position[1] - lights[j]->center[1]) 
                + sqr(Pixel_position[2] - lights[j]->center[2]));
                Object* current_object;
                for(int k = 0; objects[k] != NULL; k += 1){
                    current_object = objects[k];
                    if(current_object == object2){
                        continue;
                    }
                    double new_t = 0;
                    switch(objects[k]->kind) {
                        case 0:
                            new_t = plane_intersection(Pixel_position, object_light,
                                    objects[k]->center, objects[k]->plane.normal);
                            break;

                        case 1:
                            new_t = sphere_intersection(Pixel_position, object_light,
                                    objects[k]->center, objects[k]->sphere.radius);
                            break;
                        default:
                      // Horrible error
                            exit(1);
                    }
                    if(new_t > 0 && new_t <= dl){
                        shadow = 1;
                        break;
                    }
                }
                if (shadow == 0){
                    double diffuse[3];
                    double fang;
                    double specular[3];
                    double frad =(1/(lights[j]->light.radial2*sqr(dl) + lights[j]->light.radial1*dl + lights[j]->light.radial0*dl));
                    double R[3];
                    double L[3];
                    L[0] = lights[j]->light.direction[0];
                    L[1] = lights[j]->light.direction[1];
                    L[2] = lights[j]->light.direction[2];
                    double alpha = L[0] * lights[j]->center[0] 
                    + L[1] * lights[j]->center[1] 
                    + L[2] * lights[j]->center[2];
                    if (lights[j]->light.theta == 0){
                        fang = 1;
                    }else if (cos(lights[j]->light.theta) > cos(alpha)){
                        fang = 0;
                    }else{
                        fang = pow(cos(alpha),20);
                    }
                    if (object->kind == 1){
                        diffuse[0] = object2->sphere.difuse_color[0];
                        diffuse[1] = object2->sphere.difuse_color[1];
                        diffuse[2] = object2->sphere.difuse_color[2];
                        specular[0] = object2->sphere.specular_color[0];
                        specular[1] = object2->sphere.specular_color[1];
                        specular[2] = object2->sphere.specular_color[2];
                    }else if (object->kind == 0){
                        diffuse[0] = object2->plane.difuse_color[0];
                        diffuse[1] = object2->plane.difuse_color[1];
                        diffuse[2] = object2->plane.difuse_color[2];
                        specular[0] = object2->plane.specular_color[0];
                        specular[1] = object2->plane.specular_color[1];
                        specular[2] = object2->plane.specular_color[2];
                    }else{
                        fprintf(stderr, "Type of object does not exist");
                    }
                    
                    R[0] = light_object[0] - 2 * (N[0] * light_object[0] + N[1] * light_object[1] + N[2] * light_object[2]) * N[0];
                    R[1] = light_object[1] - 2 * (N[0] * light_object[0] + N[1] * light_object[1] + N[2] * light_object[2]) * N[1];
                    R[2] = light_object[2] - 2 * (N[0] * L[0] + N[1] * L[1] + N[2] * L[2]) * N[2];
                    normalize(R);
                    normalize(L);
                    double difuse = (N[0] * object_light[0] + N[1] * object_light[1] + N[2] * object_light[2]);
                    double specular2 = (R[0] * object_position[0] + R[1] * object_position[1] + object_position[2] * R[2]);
                    if(difuse <= 0){
                        difuse = 0;
                    }
                    if (specular2 <= 0 && difuse <= 0){
                        specular2 = 0;
                    }
                    double specular3 = pow(specular2, 20);
                    color[0] += frad*fang*((lights[j]->color[0]*difuse*diffuse[0]) + (lights[j]->color[0] * specular3 * specular[0]));
                    color[1] += frad*fang*((lights[j]->color[1]*difuse*diffuse[1]) + (lights[j]->color[1] * specular3 * specular[1]));
                    color[2] += frad*fang*((lights[j]->color[2]*difuse*diffuse[2]) + (lights[j]->color[2] * specular3 * specular[2]));
                }
            }
            image[index].r = (unsigned char)(clamp(color[0])*MAXCOLOR);
            image[index].g = (unsigned char)(clamp(color[1])*MAXCOLOR);
            image[index].b = (unsigned char)(clamp(color[2])*MAXCOLOR);
        }else{
            image[index].r = 255;
            image[index].g = 255;
            image[index].b = 255;
        }
        index++;
        }
    }
        
        
  //output to file
  outputfile = fopen(argv[4], "w");
  fprintf(outputfile, "P6\n");
  fprintf(outputfile, "%d %d\n", M, N);
  fprintf(outputfile, "%d\n", MAXCOLOR);
  fwrite(image, sizeof(Pixel), M*N, outputfile);
  return 0;
}
//set camera view
void set_camera(FILE* json){
    int c;
    skip_ws(json);
    camera.center[0] = 0;
    camera.center[1] = 0;
    camera.center[2] = 0;
      while (1) {
	// , }
	c = next_c(json);
        
	if (c == '}') {
	  // stop parsing this object
	  break;
	} else if (c == ',') {
            
	  // read another field
	  skip_ws(json);
	  char* key = next_string(json);
	  skip_ws(json);
	  expect_c(json, ':');
	  skip_ws(json);
          double value = next_number(json);
	  if (strcmp(key, "width") == 0) {
              camera.camera.width = value;
            
	  } else if ((strcmp(key, "height") == 0)) {
              camera.camera.height = value;
            
	  } else {
	    fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
		    key, line);
            exit(1);
	    //char* value = next_string(json);
	  }
        }
      }
}
