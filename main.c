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

//Structs
typedef struct {
  int kind; // 0 = cylinder, 1 = sphere
  double color[3];
  double center[3];
  union {
    struct {
      double normal[3];
    } plane;
    struct {
      double radius;
    } sphere;
    struct {
      double height;
      double width;
    } camera;
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
    #define MAXCOLOR 255 
    void set_camera(FILE* json);
    
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
  while (1) {
    c = fgetc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      return;
    }
    if (c == '{') {
        objects[i] = malloc(sizeof(Object));
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
              }
              //Checks for position,color, and normal fields
            } else if ((strcmp(key, "color") == 0) ||
                       (strcmp(key, "position") == 0) ||
                       (strcmp(key, "normal") == 0)) {
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
                  }else{
                      fprintf(stderr, "Non-valid field entered for a plane");
                      exit(1);
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
      }
      
      skip_ws(json);
      c = next_c(json);
      if (c == ',') {
	// noop
	skip_ws(json);
      } else if (c == ']') {
          objects[i] = NULL;
	fclose(json);
	return;
      } else {
	fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
	exit(1);
      }
    }
  }
  
}



int main(int argc, char** argv) {
    objects = malloc(sizeof(Object*)*129);
    int index = 0;
    FILE* outputfile;
    //checks for number of arguments
    if(argc != 5){
        fprintf(stderr, "Please put the commands in the following format: height, weight, source file, destination file.");
        exit(1);
    }
    read_scene(argv[3]);
    Object* object;
  
  double cx = 0;
  double cy = 0;
//grabs height and width of pixel
  int M = atoi(argv[2]);
  int N = atoi(argv[1]);
  if(M <= 0 || N <= 0){
      fprintf(stderr, "Please make Height and Width a positive integer.");
      exit(1);
  }
  image = malloc(sizeof(Pixel)*M*N);
  double pixheight = h / M;
  double pixwidth = w / N;
  
  //Set the objects into the proper place and set the image pixels
  for (int y = 0; y < M; y += 1) {
      for (int x = 0; x < N; x += 1) {
        double Ro[3] = {0, 0, 0};
        // Rd = normalize(P - Ro)
        double Rd[3] = {
          cx - (w/2) + pixwidth * (x + 0.5),
          cy - (h/2) + pixheight * (y + 0.5),
          1
        };
        normalize(Rd);

        double best_t = INFINITY;
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
              object = malloc(sizeof(Object));
              memcpy(object, objects[i], sizeof(Object));
          } 
        }
        //set the color for the pixel
        if (best_t > 0 && best_t != INFINITY) {
            image[index].r = (unsigned char)(object->color[0]*MAXCOLOR);
            image[index].g = (unsigned char)(object->color[1]*MAXCOLOR);
            image[index].b = (unsigned char)(object->color[2]*MAXCOLOR);
          }else{
              image[index].r = 0;
              image[index].g = 0;
              image[index].b = 0;
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
          
	  if (strcmp(key, "width") == 0) {
	    w = next_number(json);
            
	  } else if ((strcmp(key, "height") == 0)) {
	    h = next_number(json);
            
	  } else {
	    fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
		    key, line);
            exit(1);
	    //char* value = next_string(json);
	  }
        }
      }
}
