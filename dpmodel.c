
// converter for .smd files (and a .txt script) to .dpm
// written by Forest 'LordHavoc' Hale but placed into public domain
//
// disclaimer: Forest Hale is not not responsible if this code blinds you with
// its horrible design, sets your house on fire, makes you cry,
// or anything else - use at your own risk.
//
// Yes, this is perhaps my second worst code ever (next to zmodel).

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.1415926535
#endif

#if _MSC_VER
#pragma warning (disable : 4244)
#endif

#define MAX_FILEPATH 1024
#define MAX_NAME 32
#define MAX_FRAMES 65536
#define MAX_TRIS 65536
#define MAX_VERTS (MAX_TRIS * 3)
#define MAX_BONES 256
#define MAX_SHADERS 256
#define MAX_FILESIZE (64*1024*1024)
#define MAX_ATTACHMENTS MAX_BONES
// model format related flags
#define DPMBONEFLAG_ATTACH 1

char outputdir_name[MAX_FILEPATH];
char output_name[MAX_FILEPATH];
char header_name[MAX_FILEPATH];
char model_name[MAX_FILEPATH];
char scene_name[MAX_FILEPATH];
char model_name_uppercase[MAX_FILEPATH];
char scene_name_uppercase[MAX_FILEPATH];

FILE *headerfile = NULL;

double modelorigin[3], modelscale;

// this makes it keep all bones, not removing unused ones (as they might be used for attachments)
int keepallbones = 1;

void stringtouppercase(char *in, char *out)
{
	// cleanup name
	while (*in)
	{
		*out = *in++;
		// force lowercase
		if (*out >= 'a' && *out <= 'z')
			*out += 'A' - 'a';
		out++;
	}
	*out++ = 0;
}


void cleancopyname(char *out, char *in, int size)
{
	char *end = out + size - 1;
	// cleanup name
	while (out < end)
	{
		*out = *in++;
		if (!*out)
			break;
		// force lowercase
		if (*out >= 'A' && *out <= 'Z')
			*out += 'a' - 'A';
		// convert backslash to slash
		if (*out == '\\')
			*out = '/';
		out++;
	}
	end++;
	while (out < end)
		*out++ = 0; // pad with nulls
}

void chopextension(char *text)
{
	char *temp;
	if (!*text)
		return;
	temp = text;
	while (*temp)
	{
		if (*temp == '\\')
			*temp = '/';
		temp++;
	}
	temp = text + strlen(text) - 1;
	while (temp >= text)
	{
		if (*temp == '.') // found an extension
		{
			// clear extension
			*temp++ = 0;
			while (*temp)
				*temp++ = 0;
			break;
		}
		if (*temp == '/') // no extension but hit path
			break;
		temp--;
	}
}

void makepath(char *text)
{
	char *temp;
	if (!*text)
		return;
	temp = text;
	while (*temp)
	{
		if (*temp == '\\')
			*temp = '/';
		temp++;
	}
	temp = text + strlen(text) - 1;
	while (temp >= text)
	{
		if (*temp == '/') // found path
		{
			// clear filename
			temp++;
			while (*temp)
				*temp++ = 0;
			break;
		}
		temp--;
	}
}

void *readfile(char *filename, int *filesize)
{
	FILE *file;
	void *mem;
	size_t size;
	if (!filename[0])
	{
		printf("readfile: tried to open empty filename\n");
		return NULL;
	}
	if (!(file = fopen(filename, "rb")))
		return NULL;
	fseek(file, 0, SEEK_END);
	if (!(size = ftell(file)))
	{
		fclose(file);
		return NULL;
	}
	if (!(mem = malloc(size + 1)))
	{
		fclose(file);
		return NULL;
	}
	((unsigned char *)mem)[size] = 0; // 0 byte added on the end
	fseek(file, 0, SEEK_SET);
	if (fread(mem, 1, size, file) < size)
	{
		fclose(file);
		free(mem);
		return NULL;
	}
	fclose(file);
	if (filesize) // can be passed NULL...
		*filesize = size;
	printf( "READ: %s of size %i\n", filename, size );
	return mem;
}

void writefile(char *filename, void *buffer, int size)
{
	int size1;
	FILE *file;
	file = fopen(filename, "wb");
	if (!file)
	{
		printf("unable to open file \"%s\" for writing\n", filename);
		return;
	}
	size1 = fwrite(buffer, 1, size, file);
	fclose(file);
	if (size1 < size)
	{
		printf("unable to write file \"%s\"\n", filename);
		return;
	}
}

char *scriptbytes, *scriptend;
int scriptsize;

/*
int scriptfiles = 0;
char *scriptfile[256];

struct
{
	double framerate;
	int noloop;
	int skip;
}
scriptfileinfo[256];

int parsefilenames(void)
{
	char *in, *out, *name;
	scriptfiles = 0;
	in = scriptbytes;
	out = scriptfilebuffer;
	while (*in && *in <= ' ')
		in++;
	while (*in &&
	while (scriptfiles < 256)
	{
		scriptfile[scriptfiles++] = name;
		while (*in && *in != '\n' && *in != '\r')
			in++;
		if (!*in)
			break;

		while (*b > ' ')
			b++;
		while (*b && *b <= ' ')
			*b++ = 0;
		if (*b == 0)
			break;
	}
	return scriptfiles;
}
*/

unsigned char *tokenpos;

int getline(unsigned char *line)
{
	unsigned char *out = line;
	while (*tokenpos == '\r' || *tokenpos == '\n')
		tokenpos++;
	if (*tokenpos == 0)
	{
		*out++ = 0;
		return 0;
	}
	while (*tokenpos && *tokenpos != '\r' && *tokenpos != '\n')
		*out++ = *tokenpos++;
	*out++ = 0;
	return out - line;
}

typedef struct bonepose_s
{
	double m[3][4];
}
bonepose_t;

typedef struct bone_s
{
	char name[MAX_NAME];
	int parent; // parent of this bone
	int flags;
	int users; // used to determine if a bone is used to avoid saving out unnecessary bones
	int defined;
}
bone_t;

typedef struct frame_s
{
	int defined;
	char name[MAX_NAME];
	double mins[3], maxs[3], yawradius, allradius; // clipping
	int numbones;
	bonepose_t *bones;
}
frame_t;

typedef struct tripoint_s
{
	int shadernum;
	int bonenum;
	double texcoord[2];
	double origin[3];
	double normal[3];
}
tripoint;

typedef struct triangle_s
{
	int shadernum;
	int v[3];
}
triangle;

typedef struct attachment_s
{
	char name[MAX_NAME];
	char parentname[MAX_NAME];
	bonepose_t matrix;
}
attachment;

int numattachments = 0;
attachment attachments[MAX_ATTACHMENTS];

int numframes = 0;
frame_t frames[MAX_FRAMES];
int numbones = 0;
bone_t bones[MAX_BONES]; // master bone list
int numshaders = 0;
char shaders[MAX_SHADERS][32];
int numtriangles = 0;
triangle triangles[MAX_TRIS];
int numverts = 0;
tripoint vertices[MAX_VERTS];

//these are used while processing things
bonepose_t bonematrix[MAX_BONES];
char *modelfile;
int vertremap[MAX_VERTS];

double wrapangles(double f)
{
	while (f < M_PI)
		f += M_PI * 2;
	while (f >= M_PI)
		f -= M_PI * 2;
	return f;
}

bonepose_t computebonematrix(double x, double y, double z, double a, double b, double c)
{
	bonepose_t out;
	double sr, sp, sy, cr, cp, cy;

	sy = sin(c);
	cy = cos(c);
	sp = sin(b);
	cp = cos(b);
	sr = sin(a);
	cr = cos(a);

	out.m[0][0] = cp*cy;
	out.m[1][0] = cp*sy;
	out.m[2][0] = -sp;
	out.m[0][1] = sr*sp*cy+cr*-sy;
	out.m[1][1] = sr*sp*sy+cr*cy;
	out.m[2][1] = sr*cp;
	out.m[0][2] = (cr*sp*cy+-sr*-sy);
	out.m[1][2] = (cr*sp*sy+-sr*cy);
	out.m[2][2] = cr*cp;
	out.m[0][3] = x;
	out.m[1][3] = y;
	out.m[2][3] = z;
	return out;
}

bonepose_t concattransform(bonepose_t in1, bonepose_t in2)
{
	bonepose_t out;
	out.m[0][0] = in1.m[0][0] * in2.m[0][0] + in1.m[0][1] * in2.m[1][0] + in1.m[0][2] * in2.m[2][0];
	out.m[0][1] = in1.m[0][0] * in2.m[0][1] + in1.m[0][1] * in2.m[1][1] + in1.m[0][2] * in2.m[2][1];
	out.m[0][2] = in1.m[0][0] * in2.m[0][2] + in1.m[0][1] * in2.m[1][2] + in1.m[0][2] * in2.m[2][2];
	out.m[0][3] = in1.m[0][0] * in2.m[0][3] + in1.m[0][1] * in2.m[1][3] + in1.m[0][2] * in2.m[2][3] + in1.m[0][3];
	out.m[1][0] = in1.m[1][0] * in2.m[0][0] + in1.m[1][1] * in2.m[1][0] + in1.m[1][2] * in2.m[2][0];
	out.m[1][1] = in1.m[1][0] * in2.m[0][1] + in1.m[1][1] * in2.m[1][1] + in1.m[1][2] * in2.m[2][1];
	out.m[1][2] = in1.m[1][0] * in2.m[0][2] + in1.m[1][1] * in2.m[1][2] + in1.m[1][2] * in2.m[2][2];
	out.m[1][3] = in1.m[1][0] * in2.m[0][3] + in1.m[1][1] * in2.m[1][3] + in1.m[1][2] * in2.m[2][3] + in1.m[1][3];
	out.m[2][0] = in1.m[2][0] * in2.m[0][0] + in1.m[2][1] * in2.m[1][0] + in1.m[2][2] * in2.m[2][0];
	out.m[2][1] = in1.m[2][0] * in2.m[0][1] + in1.m[2][1] * in2.m[1][1] + in1.m[2][2] * in2.m[2][1];
	out.m[2][2] = in1.m[2][0] * in2.m[0][2] + in1.m[2][1] * in2.m[1][2] + in1.m[2][2] * in2.m[2][2];
	out.m[2][3] = in1.m[2][0] * in2.m[0][3] + in1.m[2][1] * in2.m[1][3] + in1.m[2][2] * in2.m[2][3] + in1.m[2][3];
	return out;
}

void transform(double in[3], bonepose_t matrix, double out[3])
{
	out[0] = in[0] * matrix.m[0][0] + in[1] * matrix.m[0][1] + in[2] * matrix.m[0][2] + matrix.m[0][3];
	out[1] = in[0] * matrix.m[1][0] + in[1] * matrix.m[1][1] + in[2] * matrix.m[1][2] + matrix.m[1][3];
	out[2] = in[0] * matrix.m[2][0] + in[1] * matrix.m[2][1] + in[2] * matrix.m[2][2] + matrix.m[2][3];
}

void inversetransform(double in[3], bonepose_t matrix, double out[3])
{
	double temp[3];
	temp[0] = in[0] - matrix.m[0][3];
	temp[1] = in[1] - matrix.m[1][3];
	temp[2] = in[2] - matrix.m[2][3];
	out[0] = temp[0] * matrix.m[0][0] + temp[1] * matrix.m[1][0] + temp[2] * matrix.m[2][0];
	out[1] = temp[0] * matrix.m[0][1] + temp[1] * matrix.m[1][1] + temp[2] * matrix.m[2][1];
	out[2] = temp[0] * matrix.m[0][2] + temp[1] * matrix.m[1][2] + temp[2] * matrix.m[2][2];
}

/*
void rotate(double in[3], bonepose_t matrix, double out[3])
{
	out[0] = in[0] * matrix.m[0][0] + in[1] * matrix.m[0][1] + in[2] * matrix.m[0][2];
	out[1] = in[0] * matrix.m[1][0] + in[1] * matrix.m[1][1] + in[2] * matrix.m[1][2];
	out[2] = in[0] * matrix.m[2][0] + in[1] * matrix.m[2][1] + in[2] * matrix.m[2][2];
}
*/

void inverserotate(double in[3], bonepose_t matrix, double out[3])
{
	out[0] = in[0] * matrix.m[0][0] + in[1] * matrix.m[1][0] + in[2] * matrix.m[2][0];
	out[1] = in[0] * matrix.m[0][1] + in[1] * matrix.m[1][1] + in[2] * matrix.m[2][1];
	out[2] = in[0] * matrix.m[0][2] + in[1] * matrix.m[1][2] + in[2] * matrix.m[2][2];
}

int parsenodes(void)
{
	int num, parent;
	unsigned char line[1024], name[1024];
	memset(bones, 0, sizeof(bones));
	numbones = 0;
	while (getline(line))
	{
		if (!strcmp(line, "end"))
			break;
		// warning: this is just begging for an overflow exploit, but who would bother overflowing a model converter?
		if (sscanf(line, "%i \"%[^\"]\" %i", &num, name, &parent) != 3)
		{
			printf("error in nodes data");
			return 0;
		}
		if (num < 0 || num >= MAX_BONES)
		{
			printf("invalid bone number %i\n", num);
			return 0;
		}
		if (parent >= num)
		{
			printf("bone's parent >= bone's number\n");
			return 0;
		}
		if (parent < -1)
		{
			printf("bone's parent < -1\n");
			return 0;
		}
		if (parent >= 0 && !bones[parent].defined)
		{
			printf("bone's parent bone has not been defined\n");
			return 0;
		}
		cleancopyname(name, name, MAX_NAME);
		memcpy(bones[num].name, name, MAX_NAME);
		bones[num].defined = 1;
		bones[num].parent = parent;
		if (num >= numbones)
			numbones = num + 1;
	}
	return 1;
}

int parseskeleton(void)
{
	unsigned char line[1024], command[256], temp[1024];
	int i, frame, num;
	double x, y, z, a, b, c;
	int baseframe;
	baseframe = numframes;
	frame = baseframe;
	while (getline(line))
	{
		sscanf(line, "%s %i", command, &i);
		if (!strcmp(command, "end"))
			break;
		if (!strcmp(command, "time"))
		{
			if (i < 0)
			{
				printf("invalid time %i\n", i);
				return 0;
			}
			frame = baseframe + i;
			if (frame >= MAX_FRAMES)
			{
				printf("only %i frames supported currently\n", MAX_FRAMES);
				return 0;
			}
			if (frames[frame].defined)
			{
				printf("warning: duplicate frame\n");
				free(frames[frame].bones);
			}
			sprintf(temp, "%s_%i", scene_name, i);
			if (strlen(temp) > 31)
			{
				printf("error: frame name \"%s\" is longer than 31 characters\n", temp);
				return 0;
			}
			cleancopyname(frames[frame].name, temp, MAX_NAME);
			frames[frame].numbones = numbones + numattachments + 1;
			frames[frame].bones = malloc(frames[frame].numbones * sizeof(bonepose_t));
			memset(frames[frame].bones, 0, frames[frame].numbones * sizeof(bonepose_t));
			frames[frame].bones[frames[frame].numbones - 1].m[0][1] = 35324;
			frames[frame].defined = 1;
			if (numframes < frame + 1)
				numframes = frame + 1;
		}
		else
		{
			if (sscanf(line, "%i %lf %lf %lf %lf %lf %lf", &num, &x, &y, &z, &a, &b, &c) != 7)
			{
				printf("invalid bone pose \"%s\"\n", line);
				return 0;
			}
			if (num < 0 || num >= numbones)
			{
				printf("error: invalid bone number: %i\n", num);
				return 0;
			}
			if (!bones[num].defined)
			{
				printf("error: bone %i not defined\n", num);
				return 0;
			}
			// root bones need to be offset
			if (bones[num].parent < 0)
			{
				x = (x - modelorigin[0]) * modelscale;
				y = (y - modelorigin[1]) * modelscale;
				z = (z - modelorigin[2]) * modelscale;
			}
			// LordHavoc: compute matrix
			frames[frame].bones[num] = computebonematrix(x, y, z, a, b, c);
		}
	}

	for (frame = 0;frame < numframes;frame++)
	{
		if (!frames[frame].defined)
		{
			if (frame < 1)
			{
				printf("error: no first frame\n");
				return 0;
			}
			if (!frames[frame - 1].defined)
			{
				printf("error: no previous frame to duplicate\n");
				return 0;
			}
			sprintf(temp, "%s_%i", scene_name, frame - baseframe);
			if (strlen(temp) > 31)
			{
				printf("error: frame name \"%s\" is longer than 31 characters\n", temp);
				return 0;
			}
			printf("frame %s missing, duplicating previous frame %s with new name %s\n", temp, frames[frame - 1].name, temp);
			frames[frame].defined = 1;
			cleancopyname(frames[frame].name, temp, MAX_NAME);
			frames[frame].numbones = numbones + numattachments + 1;
			frames[frame].bones = malloc(frames[frame].numbones * sizeof(bonepose_t));
			memcpy(frames[frame].bones, frames[frame - 1].bones, frames[frame].numbones * sizeof(bonepose_t));
			frames[frame].bones[frames[frame].numbones - 1].m[0][1] = 35324;
			printf("duplicate frame named %s\n", frames[frame].name);
		}
		if (frame >= baseframe && headerfile)
			fprintf(headerfile, "#define MODEL_%s_%s_%i %i\n", model_name_uppercase, scene_name_uppercase, frame - baseframe, frame);
	}
	if (headerfile)
	{
		fprintf(headerfile, "#define MODEL_%s_%s_START %i\n", model_name_uppercase, scene_name_uppercase, baseframe);
		fprintf(headerfile, "#define MODEL_%s_%s_END %i\n", model_name_uppercase, scene_name_uppercase, numframes);
		fprintf(headerfile, "#define MODEL_%s_%s_LENGTH %i\n", model_name_uppercase, scene_name_uppercase, numframes - baseframe);
		fprintf(headerfile, "\n");
	}
	return 1;
}

/*
int sentinelcheckframes(char *filename, int fileline)
{
	int i;
	for (i = 0;i < numframes;i++)
	{
		if (frames[i].defined && frames[i].bones)
		{
			if (frames[i].bones[frames[i].numbones - 1].m[0][1] != 35324)
			{
				printf("sentinelcheckframes: error on frame %s detected at %s:%i\n", frames[i].name, filename, fileline);
				exit(1);
			}
		}
	}
	return 1;
}
*/

int freeframes(void)
{
	int i;
	//sentinelcheckframes(__FILE__, __LINE__);
	//printf("no errors were detected\n");
	for (i = 0;i < numframes;i++)
	{
		if (frames[i].defined && frames[i].bones)
		{
			//fprintf(stdout, "freeing %s\n", frames[i].name);
			//fflush(stdout);
			//if (frames[i].bones[frames[i].numbones - 1].m[0][1] != 35324)
			//	printf("freeframes: error on frame %s\n", frames[i].name);
			//else
				free(frames[i].bones);
		}
	}
	numframes = 0;
	return 1;
}

int initframes(void)
{
	memset(frames, 0, sizeof(frames));
	return 1;
}

int parsetriangles(void)
{
	unsigned char line[1024];
	int current = 0, i, found = 0;
	double org[3], normal[3];
	double d;
	int vbonenum;
	double vorigin[3], vnormal[3], vtexcoord[2];
	numtriangles = 0;
	numshaders = 0;
	for (i = 0;i < numbones;i++)
	{
		if (bones[i].parent >= 0)
			bonematrix[i] = concattransform(bonematrix[bones[i].parent], frames[0].bones[i]);
		else
			bonematrix[i] = frames[0].bones[i];
	}
	while (getline(line))
	{
		if (!strcmp(line, "end"))
			break;
		if (current == 0)
		{
			found = 0;
			for (i = 0;i < numshaders;i++) {
#ifdef WIN32
				if (!stricmp(shaders[i], line)) {
#else
				if (!strcasecmp(shaders[i], line)) {
#endif
					found = 1;
					break;
				}
			}
			triangles[numtriangles].shadernum = i;
			if (!found)
			{
				cleancopyname(shaders[i], line, MAX_NAME);
				numshaders++;
			}
			current++;
		}
		else
		{
			if (sscanf(line, "%i %lf %lf %lf %lf %lf %lf %lf %lf", &vbonenum, &org[0], &org[1], &org[2], &normal[0], &normal[1], &normal[2], &vtexcoord[0], &vtexcoord[1]) < 9)
			{
				printf("invalid vertex \"%s\"\n", line);
				return 0;
			}
			if (vbonenum < 0 || vbonenum >= MAX_BONES)
			{
				printf("invalid bone number %i in triangle data\n", vbonenum);
				return 0;
			}
			if (!bones[vbonenum].defined)
			{
				printf("bone %i in triangle data is not defined\n", vbonenum);
				return 0;
			}
			// apply model scaling and offset
			org[0] = (org[0] - modelorigin[0]) * modelscale;
			org[1] = (org[1] - modelorigin[1]) * modelscale;
			org[2] = (org[2] - modelorigin[2]) * modelscale;
			// untransform the origin and normal
			inversetransform(org, bonematrix[vbonenum], vorigin);
			inverserotate(normal, bonematrix[vbonenum], vnormal);
			d = 1 / sqrt(vnormal[0] * vnormal[0] + vnormal[1] * vnormal[1] + vnormal[2] * vnormal[2]);
			vnormal[0] *= d;
			vnormal[1] *= d;
			vnormal[2] *= d;

			// round off minor errors in the normal
			if (fabs(vnormal[0]) < 0.001)
				vnormal[0] = 0;
			if (fabs(vnormal[1]) < 0.001)
				vnormal[1] = 0;
			if (fabs(vnormal[2]) < 0.001)
				vnormal[2] = 0;
			d = 1 / sqrt(vnormal[0] * vnormal[0] + vnormal[1] * vnormal[1] + vnormal[2] * vnormal[2]);
			vnormal[0] *= d;
			vnormal[1] *= d;
			vnormal[2] *= d;

			// add vertex to list if unique
			for (i = 0;i < numverts;i++)
				if (vertices[i].shadernum == triangles[numtriangles].shadernum
				 && vertices[i].bonenum == vbonenum
				 && vertices[i].origin[0] == vorigin[0] && vertices[i].origin[1] == vorigin[1] && vertices[i].origin[2] == vorigin[2]
				 && vertices[i].normal[0] == vnormal[0] && vertices[i].normal[1] == vnormal[1] && vertices[i].normal[2] == vnormal[2]
				 && vertices[i].texcoord[0] == vtexcoord[0] && vertices[i].texcoord[1] == vtexcoord[1])
					break;
			triangles[numtriangles].v[current - 1] = i;
			if (i >= numverts)
			{
				numverts++;
				vertices[i].shadernum = triangles[numtriangles].shadernum;
				vertices[i].bonenum = vbonenum;
				vertices[i].origin[0] = vorigin[0];vertices[i].origin[1] = vorigin[1];vertices[i].origin[2] = vorigin[2];
				vertices[i].normal[0] = vnormal[0];vertices[i].normal[1] = vnormal[1];vertices[i].normal[2] = vnormal[2];
				vertices[i].texcoord[0] = vtexcoord[0];vertices[i].texcoord[1] = vtexcoord[1];
			}

			current++;
			if (current >= 4)
			{
				current = 0;
				numtriangles++;
			}
		}
	}
	return 1;
}

int parsemodelfile(void)
{
	int i;
	char line[1024], command[256];
	tokenpos = modelfile;
	while (getline(line))
	{
		sscanf(line, "%s %i", command, &i);
		if (!strcmp(command, "version"))
		{
			if (i != 1)
			{
				printf("file is version %d, only version 1 is supported\n", i);
				return 0;
			}
		}
		else if (!strcmp(command, "nodes"))
		{
			if (!parsenodes())
				return 0;
		}
		else if (!strcmp(command, "skeleton"))
		{
			if (!parseskeleton())
				return 0;
		}
		else if (!strcmp(command, "triangles"))
		{
			if (!parsetriangles())
				return 0;
		}
		else
		{
			printf("unknown command \"%s\"\n", line);
			return 0;
		}
	}
	return 1;
}

int addattachments(void)
{
	int i, j;
	//sentinelcheckframes(__FILE__, __LINE__);
	for (i = 0;i < numattachments;i++)
	{
		bones[numbones].defined = 1;
		bones[numbones].parent = -1;
		bones[numbones].flags = DPMBONEFLAG_ATTACH;
		for (j = 0;j < numbones;j++)
			if (!strcmp(bones[j].name, attachments[i].parentname))
				bones[numbones].parent = j;
		if (bones[numbones].parent < 0)
			printf("warning: unable to find bone \"%s\" for attachment \"%s\", using root instead\n", attachments[i].parentname, attachments[i].name);
		cleancopyname(bones[numbones].name, attachments[i].name, MAX_NAME);
		// we have to duplicate the attachment in every frame
		//sentinelcheckframes(__FILE__, __LINE__);
		for (j = 0;j < numframes;j++)
			frames[j].bones[numbones] = attachments[i].matrix;
		//sentinelcheckframes(__FILE__, __LINE__);
		numbones++;
	}
	numattachments = 0;
	//sentinelcheckframes(__FILE__, __LINE__);
	return 1;
}

int cleanupbones(void)
{
	int i, j;
	int oldnumbones;
	int remap[MAX_BONES];
	//sentinelcheckframes(__FILE__, __LINE__);

	// figure out which bones are used
	for (i = 0;i < numbones;i++)
	{
		if (bones[i].defined)
		{
			bones[i].users = 0;
			if (bones[i].flags & DPMBONEFLAG_ATTACH)
				bones[i].users++;
		}
	}
	for (i = 0;i < numverts;i++)
		bones[vertices[i].bonenum].users++;
	for (i = 0;i < numbones;i++)
		if (bones[i].defined && bones[i].users && bones[i].parent >= 0)
			bones[bones[i].parent].users++;

	// now calculate the remapping table for whichever ones should remain
	oldnumbones = numbones;
	numbones = 0;
	for (i = 0;i < oldnumbones;i++)
	{
		if (bones[i].defined && bones[i].users)
			remap[i] = numbones++;
		else
		{
			remap[i] = -1;
			//for (j = 0;j < numframes;j++)
			//	memset(&frames[j].bones[i], 0, sizeof(bonepose_t));
		}
	}

	//sentinelcheckframes(__FILE__, __LINE__);
	// shuffle bone data around to eliminate gaps
	for (i = 0;i < oldnumbones;i++)
		if (bones[i].parent >= 0)
			bones[i].parent = remap[bones[i].parent];
	for (i = 0;i < oldnumbones;i++)
		if (remap[i] >= 0 && remap[i] != i)
			bones[remap[i]] = bones[i];
	//sentinelcheckframes(__FILE__, __LINE__);
	for (i = 0;i < numframes;i++)
	{
		if (frames[i].defined)
		{
			//sentinelcheckframes(__FILE__, __LINE__);
			for (j = 0;j < oldnumbones;j++)
			{
				if (remap[j] >= 0 && remap[j] != j)
				{
					//printf("copying bone %i to %i\n", j, remap[j]);
					//sentinelcheckframes(__FILE__, __LINE__);
					frames[i].bones[remap[j]] = frames[i].bones[j];
					//sentinelcheckframes(__FILE__, __LINE__);
				}
			}
			//sentinelcheckframes(__FILE__, __LINE__);
		}
	}
	//sentinelcheckframes(__FILE__, __LINE__);

	// remap vertex references
	for (i = 0;i < numverts;i++)
		vertices[i].bonenum = remap[vertices[i].bonenum];

	//sentinelcheckframes(__FILE__, __LINE__);
	return 1;
}

int cleanupframes(void)
{
	int i, j/*, best*/;
	double org[3], dist, mins[3], maxs[3], yawradius, allradius;
	for (i = 0;i < numframes;i++)
	{
		//sentinelcheckframes(__FILE__, __LINE__);
		for (j = 0;j < numbones;j++)
		{
			if (bones[j].defined)
			{
				if (bones[j].parent >= 0)
					bonematrix[j] = concattransform(bonematrix[bones[j].parent], frames[i].bones[j]);
				else
					bonematrix[j] = frames[i].bones[j];
			}
		}
		mins[0] = mins[1] = mins[2] = 0;
		maxs[0] = maxs[1] = maxs[2] = 0;
		yawradius = 0;
		allradius = 0;
		//best = 0;
		for (j = 0;j < numverts;j++)
		{
			transform(vertices[j].origin, bonematrix[vertices[j].bonenum], org);
			if (mins[0] > org[0]) mins[0] = org[0];
			if (mins[1] > org[1]) mins[1] = org[1];
			if (mins[2] > org[2]) mins[2] = org[2];
			if (maxs[0] < org[0]) maxs[0] = org[0];
			if (maxs[1] < org[1]) maxs[1] = org[1];
			if (maxs[2] < org[2]) maxs[2] = org[2];
			dist = org[0]*org[0]+org[1]*org[1];
			if (yawradius < dist)
				yawradius = dist;
			dist += org[2]*org[2];
			if (allradius < dist)
			{
		//		best = j;
				allradius = dist;
			}
		}
		/*
		j = best;
		transform(vertices[j].origin, bonematrix[vertices[j].bonenum], org);
		printf("furthest vert of frame %s is on bone %s (%i), matrix is:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\nvertex origin %f %f %f - %f %f %f\nbbox %f %f %f - %f %f %f - %f %f\n", frames[i].name, bones[vertices[j].bonenum].name, vertices[j].bonenum
		, bonematrix[vertices[j].bonenum].m[0][0], bonematrix[vertices[j].bonenum].m[0][1], bonematrix[vertices[j].bonenum].m[0][2], bonematrix[vertices[j].bonenum].m[0][3]
		, bonematrix[vertices[j].bonenum].m[1][0], bonematrix[vertices[j].bonenum].m[1][1], bonematrix[vertices[j].bonenum].m[1][2], bonematrix[vertices[j].bonenum].m[1][3]
		, bonematrix[vertices[j].bonenum].m[2][0], bonematrix[vertices[j].bonenum].m[2][1], bonematrix[vertices[j].bonenum].m[2][2], bonematrix[vertices[j].bonenum].m[2][3]
		, vertices[j].origin[0], vertices[j].origin[1], vertices[j].origin[2], org[0], org[1], org[2]
		, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], sqrt(yawradius), sqrt(allradius));
		*/
		frames[i].mins[0] = mins[0];
		frames[i].mins[1] = mins[1];
		frames[i].mins[2] = mins[2];
		frames[i].maxs[0] = maxs[0];
		frames[i].maxs[1] = maxs[1];
		frames[i].maxs[2] = maxs[2];
		frames[i].yawradius = sqrt(yawradius);
		frames[i].allradius = sqrt(allradius);
		//sentinelcheckframes(__FILE__, __LINE__);
	}
	return 1;
}

int cleanupshadernames(void)
{
	int i;
	//char temp[1024];
	for (i = 0;i < numshaders;i++)
	{
		/*
		sprintf(temp, "%s%s", texturedir, shaders[i]);
		chopextension(temp);
		if (strlen(temp) >= MAX_NAME)
		{
			printf("shader name too long %s\n", temp);
			return 0;
		}
		cleancopyname(shaders[i], temp, MAX_NAME);
		*/
		chopextension(shaders[i]);
	}
	return 1;
}

char *token;

void inittokens(char *script)
{
	token = script;
}

char tokenbuffer[1024];

char *gettoken(void)
{
	char *out;
	out = tokenbuffer;
	while (*token && *token <= ' ' && *token != '\n')
		token++;
	if (!*token)
		return NULL;
	switch (*token)
	{
	case '\"':
		token++;
		while (*token && *token != '\r' && *token != '\n' && *token != '\"')
			*out++ = *token++;
		*out++ = 0;
		if (*token == '\"')
			token++;
		else
			printf("warning: unterminated quoted string\n");
		return tokenbuffer;
	case '(':
	case ')':
	case '{':
	case '}':
	case '[':
	case ']':
	case '\n':
		tokenbuffer[0] = *token++;
		tokenbuffer[1] = 0;
		return tokenbuffer;
	default:
		while (*token && *token > ' ' && *token != '(' && *token != ')' && *token != '{' && *token != '}' && *token != '[' && *token != ']' && *token != '\"')
			*out++ = *token++;
		*out++ = 0;
		return tokenbuffer;
	}
}

typedef struct sccommand_s
{
	char *name;
	int (*code)(void);
}
sccommand;

int isdouble(char *c)
{
	while (*c)
	{
		switch (*c)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '.':
		case 'e':
		case 'E':
		case '-':
		case '+':
			break;
		default:
			return 0;
		}
		c++;
	}
	return 1;
}

int isfilename(char *c)
{
	while (*c)
	{
		if (*c < ' ')
			return 0;
		c++;
	}
	return 1;
}

int sc_attachment(void)
{
	int i;
	char *c;
	double origin[3], angles[3];
	if (numattachments >= MAX_ATTACHMENTS)
	{
		printf("ran out of attachment slots\n");
		return 0;
	}
	c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	cleancopyname(attachments[numattachments].name, c, MAX_NAME);

	c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	cleancopyname(attachments[numattachments].parentname, c, MAX_NAME);

	for (i = 0;i < 6;i++)
	{
		c = gettoken();
		if (!c)
			return 0;
		if (!isdouble(c))
			return 0;
		if (i < 3)
			origin[i] = atof(c);
		else
			angles[i - 3] = atof(c) * (M_PI / 180.0);
	}
	attachments[numattachments].matrix = computebonematrix(origin[0], origin[1], origin[2], angles[0], angles[1], angles[2]);

	numattachments++;
	return 1;
}

int sc_outputdir(void)
{
	char *c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	strcpy(outputdir_name, c);
	chopextension(outputdir_name);
	if (strlen(outputdir_name) && outputdir_name[strlen(outputdir_name) - 1] != '/')
		strcat(outputdir_name, "/");
	return 1;
}

int sc_model(void)
{
	char *c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	strcpy(model_name, c);
	chopextension(model_name);
	stringtouppercase(model_name, model_name_uppercase);

	sprintf(header_name, "%s%s.h", outputdir_name, model_name);
	sprintf(output_name, "%s%s.dpm", outputdir_name, model_name);
	return 1;
}

/*
int sc_texturedir(void)
{
	char *c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	strcpy(texturedir, c);
	if (strlen(texturedir) && texturedir[strlen(texturedir) - 1] != '/')
		strcat(texturedir, "/");
	if (texturedir[0] == '/')
	{
		printf("texturedir not allowed to begin with \"/\" (could access parent directories)\n");
		printf("normally a texturedir should be the same name as the model it is for\n");
		return 0;
	}
	if (strstr(texturedir, ":"))
	{
		printf("\":\" not allowed in texturedir (could access parent directories)\n");
		printf("normally a texturedir should be the same name as the model it is for\n");
		return 0;
	}
	if (strstr(texturedir, "."))
	{
		printf("\".\" not allowed in texturedir (could access parent directories)\n");
		printf("normally a texturedir should be the same name as the model it is for\n");
		return 0;
	}
	if (strstr(texturedir, "\\"))
	{
		printf("\"\\\" not allowed in texturedir (use / instead)\n");
		printf("normally a texturedir should be the same name as the model it is for\n");
		return 0;
	}
	return 1;
}
*/

int sc_origin(void)
{
	int i;
	char *c;
	for (i = 0;i < 3;i++)
	{
		c = gettoken();
		if (!c)
			return 0;
		if (!isdouble(c))
			return 0;
		modelorigin[i] = atof(c);
	}
	return 1;
}

int sc_scale(void)
{
	char *c = gettoken();
	if (!c)
		return 0;
	if (!isdouble(c))
		return 0;
	modelscale = atof(c);
	return 1;
}

int sc_scene(void)
{
	char *c;
	c = gettoken();
	if (!c)
		return 0;
	if (!isfilename(c))
		return 0;
	modelfile = readfile(c, NULL);
	if (!modelfile)
		return 0;
	cleancopyname(scene_name, c, MAX_NAME);
	chopextension(scene_name);
	stringtouppercase(scene_name, scene_name_uppercase);
	printf("parsing scene %s\n", scene_name);
	if (!headerfile)
	{
		headerfile = fopen(header_name, "wb");
		if (headerfile)
		{
			fprintf(headerfile, "/*\n");
			fprintf(headerfile, "Generated header file for %s\n", model_name);
			fprintf(headerfile, "This file contains frame number definitions for use in code referencing the model, to make code more readable and maintainable.\n");
			fprintf(headerfile, "*/\n");
			fprintf(headerfile, "\n");
			fprintf(headerfile, "#ifndef MODEL_%s_H\n", model_name_uppercase);
			fprintf(headerfile, "#define MODEL_%s_H\n", model_name_uppercase);
			fprintf(headerfile, "\n");
		}
	}
	if (!parsemodelfile())
		return 0;
	free(modelfile);
	return 1;
}

int sc_comment(void)
{
	while (gettoken()[0] != '\n');
	return 1;
}

int sc_nothing(void)
{
	return 1;
}

sccommand sc_commands[] =
{
	{"attachment", sc_attachment},
	{"outputdir", sc_outputdir},
	{"model", sc_model},
//	{"texturedir", sc_texturedir},
	{"origin", sc_origin},
	{"scale", sc_scale},
	{"scene", sc_scene},
	{"#", sc_comment},
	{"\n", sc_nothing},
	{"", NULL}
};

int processcommand(char *command)
{
	int r;
	sccommand *c;
	c = sc_commands;
	while (c->name[0])
	{
		if (!strcmp(c->name, command))
		{
			printf("executing command %s\n", command);
			r = c->code();
			if (!r)
				printf("error processing script\n");
			return r;
		}
		c++;
	}
	printf("command %s not recognized\n", command);
	return 0;
}

int convertmodel(void);
void processscript(void)
{
	char *c;
	inittokens(scriptbytes);
	numframes = 0;
	numbones = 0;
	numshaders = 0;
	numtriangles = 0;
	initframes();
	while ((c = gettoken()))
		if (c[0] > ' ')
			if (!processcommand(c))
				return;
	if (headerfile)
	{
		fprintf(headerfile, "#endif /*MODEL_%s_H*/\n", model_name_uppercase);
		fclose(headerfile);
	}
	if (!addattachments())
	{
		freeframes();
		return;
	}
	if (!cleanupbones())
	{
		freeframes();
		return;
	}
	if (!cleanupframes())
	{
		freeframes();
		return;
	}
	if (!cleanupshadernames())
	{
		freeframes();
		return;
	}
	if (!convertmodel())
	{
		freeframes();
		return;
	}
	freeframes();
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage: %s scriptname.txt\n", argv[0]);
		return 0;
	}
	scriptbytes = readfile(argv[1], &scriptsize);
	if (!scriptbytes)
	{
		printf("unable to read script file\n");
		return 0;
	}
	scriptend = scriptbytes + scriptsize;
	processscript();
#if (_MSC_VER && _DEBUG)
	printf("destroy any key\n");
	getchar();
#endif
	return 0;
}

/*
bone_t tbone[MAX_BONES];
int boneusage[MAX_BONES], boneremap[MAX_BONES], boneunmap[MAX_BONES];

int numtbones;

typedef struct tvert_s
{
	int bonenum;
	double texcoord[2];
	double origin[3];
	double normal[3];
}
tvert_t;

tvert_t tvert[MAX_VERTS];
int tvertusage[MAX_VERTS];
int tvertremap[MAX_VERTS];
int tvertunmap[MAX_VERTS];

int numtverts;

int ttris[MAX_TRIS][4]; // 0, 1, 2 are vertex indices, 3 is shader number

char shaders[MAX_SHADERS][MAX_NAME];

int numshaders;

int shaderusage[MAX_SHADERS];
int shadertrisstart[MAX_SHADERS];
int shadertriscurrent[MAX_SHADERS];
int shadertris[MAX_TRIS];
*/

unsigned char *output;
unsigned char outputbuffer[MAX_FILESIZE];

void putstring(char *in, int length)
{
	while (*in && length)
	{
		*output++ = *in++;
		length--;
	}
	// pad with nulls
	while (length--)
		*output++ = 0;
}

void putnulls(int num)
{
	while (num--)
		*output++ = 0;
}

void putlong(int num)
{
	*output++ = ((num >> 24) & 0xFF);
	*output++ = ((num >> 16) & 0xFF);
	*output++ = ((num >>  8) & 0xFF);
	*output++ = ((num >>  0) & 0xFF);
}

void putfloat(double num)
{
	union
	{
		float f;
		int i;
	}
	n;

	n.f = num;
	// this matchs for both positive and negative 0, thus setting it to positive 0
	if (n.f == 0)
		n.f = 0;
	putlong(n.i);
}

void putinit(void)
{
	output = outputbuffer;
}

int putgetposition(void)
{
	return (int) output - (int) outputbuffer;
}

void putsetposition(int n)
{
	output = (unsigned char *) (n + (int) outputbuffer);
}

typedef struct lump_s
{
	int start, length;
}
lump_t;

//double posemins[MAX_FRAMES][3], posemaxs[MAX_FRAMES][3], poseradius[MAX_FRAMES];

int convertmodel(void)
{
	int i, j, k, l, nverts, ntris;
	float mins[3], maxs[3], yawradius, allradius;
	int pos_filesize, pos_lumps, pos_frames, pos_bones, pos_meshs, pos_verts, pos_texcoords, pos_index, pos_groupids, pos_framebones;
	int filesize, restoreposition;

	//sentinelcheckframes(__FILE__, __LINE__);

	putsetposition(0);

	// ID string
	putstring("DARKPLACESMODEL", 16);

	// type 2 model, hierarchical skeletal pose
	putlong(2);

	// filesize
	pos_filesize = putgetposition();
	putlong(0);

	// bounding box, cylinder, and sphere
	mins[0] = frames[0].mins[0];
	mins[1] = frames[0].mins[1];
	mins[2] = frames[0].mins[2];
	maxs[0] = frames[0].maxs[0];
	maxs[1] = frames[0].maxs[1];
	maxs[2] = frames[0].maxs[2];
	yawradius = frames[0].yawradius;
	allradius = frames[0].allradius;
	for (i = 0;i < numframes;i++)
	{
		if (mins[0] > frames[i].mins[0]) mins[0] = frames[i].mins[0];
		if (mins[1] > frames[i].mins[1]) mins[1] = frames[i].mins[1];
		if (mins[2] > frames[i].mins[2]) mins[2] = frames[i].mins[2];
		if (maxs[0] < frames[i].maxs[0]) maxs[0] = frames[i].maxs[0];
		if (maxs[1] < frames[i].maxs[1]) maxs[1] = frames[i].maxs[1];
		if (maxs[2] < frames[i].maxs[2]) maxs[2] = frames[i].maxs[2];
		if (yawradius < frames[i].yawradius) yawradius = frames[i].yawradius;
		if (allradius < frames[i].allradius) allradius = frames[i].allradius;
	}
	putfloat(mins[0]);
	putfloat(mins[1]);
	putfloat(mins[2]);
	putfloat(maxs[0]);
	putfloat(maxs[1]);
	putfloat(maxs[2]);
	putfloat(yawradius);
	putfloat(allradius);

	// numbers of things
	putlong(numbones);
	putlong(numshaders);
	putlong(numframes);

	// offsets to things
	pos_lumps = putgetposition();
	putlong(0);
	putlong(0);
	putlong(0);

	// store the bones
	pos_bones = putgetposition();
	for (i = 0;i < numbones;i++)
	{
		putstring(bones[i].name, MAX_NAME);
		putlong(bones[i].parent);
		putlong(bones[i].flags);
	}

	// store the meshs
	pos_meshs = putgetposition();
	// skip over the mesh structs, they will be filled in later
	putsetposition(pos_meshs + numshaders * (MAX_NAME + 24));

	// store the frames
	pos_frames = putgetposition();
	// skip over the frame structs, they will be filled in later
	putsetposition(pos_frames + numframes * (MAX_NAME + 36));

	// store the data referenced by meshs
	for (i = 0;i < numshaders;i++)
	{
		pos_verts = putgetposition();
		nverts = 0;
		for (j = 0;j < numverts;j++)
		{
			if (vertices[j].shadernum == i)
			{
				vertremap[j] = nverts++;
				putlong(1); // how many bones for this vertex (always 1 for smd)
				// this would be a loop if smd had more than one bone per vertex
				putfloat(vertices[j].origin[0]);
				putfloat(vertices[j].origin[1]);
				putfloat(vertices[j].origin[2]);
				putfloat(1); // influence of the bone on the vertex
				putfloat(vertices[j].normal[0]);
				putfloat(vertices[j].normal[1]);
				putfloat(vertices[j].normal[2]);
				putlong(vertices[j].bonenum); // number of the bone
			}
			else
				vertremap[j] = -1;
		}
		pos_texcoords = putgetposition();
		for (j = 0;j < numverts;j++)
		{
			if (vertices[j].shadernum == i)
			{
				// OpenGL wants bottom to top texcoords
				putfloat(vertices[j].texcoord[0]);
				putfloat(1.0f - vertices[j].texcoord[1]);
			}
		}
		pos_index = putgetposition();
		ntris = 0;
		for (j = 0;j < numtriangles;j++)
		{
			if (triangles[j].shadernum == i)
			{
				putlong(vertremap[triangles[j].v[0]]);
				putlong(vertremap[triangles[j].v[1]]);
				putlong(vertremap[triangles[j].v[2]]);
				ntris++;
			}
		}
		pos_groupids = putgetposition();
		for (j = 0;j < numtriangles;j++)
			if (triangles[j].shadernum == i)
				putlong(0);

		// now we actually write the mesh header
		restoreposition = putgetposition();
		putsetposition(pos_meshs + i * (MAX_NAME + 24));
		putstring(shaders[i], MAX_NAME);
		putlong(nverts);
		putlong(ntris);
		putlong(pos_verts);
		putlong(pos_texcoords);
		putlong(pos_index);
		putlong(pos_groupids);
		putsetposition(restoreposition);
	}

	// store the data referenced by frames
	for (i = 0;i < numframes;i++)
	{
		pos_framebones = putgetposition();
		for (j = 0;j < numbones;j++)
			for (k = 0;k < 3;k++)
				for (l = 0;l < 4;l++)
					putfloat(frames[i].bones[j].m[k][l]);

		// now we actually write the frame header
		restoreposition = putgetposition();
		putsetposition(pos_frames + i * (MAX_NAME + 36));
		putstring(frames[i].name, MAX_NAME);
		putfloat(frames[i].mins[0]);
		putfloat(frames[i].mins[1]);
		putfloat(frames[i].mins[2]);
		putfloat(frames[i].maxs[0]);
		putfloat(frames[i].maxs[1]);
		putfloat(frames[i].maxs[2]);
		putfloat(frames[i].yawradius);
		putfloat(frames[i].allradius);
		putlong(pos_framebones);
		putsetposition(restoreposition);
	}

	filesize = putgetposition();
	putsetposition(pos_lumps);
	putlong(pos_bones);
	putlong(pos_meshs);
	putlong(pos_frames);
	putsetposition(pos_filesize);
	putlong(filesize);
	putsetposition(filesize);

	// print model stats
	printf("model stats:\n");
	printf("%i vertices %i triangles %i bones %i shaders %i frames\n", numverts, numtriangles, numbones, numshaders, numframes);
	printf("renderlist:\n");
	for (i = 0;i < numshaders;i++)
	{
		nverts = 0;
		for (j = 0;j < numverts;j++)
			if (vertices[j].shadernum == i)
				nverts++;
		ntris = 0;
		for (j = 0;j < numtriangles;j++)
			if (triangles[j].shadernum == i)
				ntris++;
		printf("%5i tris%6i verts : %s\n", ntris, nverts, shaders[i]);
	}
	printf("file size: %5ik\n", (filesize + 1023) >> 10);
	writefile(output_name, outputbuffer, filesize);
	printf("wrote file %s\n", output_name);
//	printf("writing shader and textures\n");

/*
	tripoint *p;
	tvert_t *tv;
	int lumps, i, j, k, bonenum, filesizeposition;
	lump_t lump[9];
	char name[MAX_NAME + 1];
	double mins[3], maxs[3], radius, tmins[3], tmaxs[3], tradius, org[3], dist;

	// merge duplicate verts while building triangle list
	for (i = 0;i < numtriangles;i++)
	{
		for (j = 0;j < 3;j++)
		{
			p = &triangles[i].point[j];
			for (bonenum = 0;bonenum < numbones;bonenum++)
				if (!strcmp(bone[bonenum].name, p->bonename))
					break;
			for (k = 0, tv = tvert;k < numtverts;k++, tv++)
				if (tv->bonenum == bonenum
				 && tv->texcoord[0] == p->texcoord[0] && tv->texcoord[1] == p->texcoord[1]
				 && tv->origin[0] == p->origin[0] && tv->origin[1] == p->origin[1] && tv->origin[2] == p->origin[2]
//				 && tv->normal[0] == p->normal[0] && tv->normal[1] == p->normal[1] && tv->normal[2] == p->normal[2]
				 )
					goto foundtvert;
			if (k >= MAX_VERTS)
			{
				printf("ran out of all %i vertex slots\n", MAX_VERTS);
				return 0;
			}
			tv->bonenum = bonenum;
			tv->texcoord[0] = p->texcoord[0];
			tv->texcoord[1] = p->texcoord[1];
			tv->origin[0] = p->origin[0];
			tv->origin[1] = p->origin[1];
			tv->origin[2] = p->origin[2];
			tv->normal[0] = p->normal[0];
			tv->normal[1] = p->normal[1];
			tv->normal[2] = p->normal[2];
			numtverts++;
foundtvert:
			ttris[i][j] = k;
		}

		cleancopyname(name, triangles[i].texture, MAX_NAME);
		for (k = 0;k < numshaders;k++)
		{
			if (!memcmp(shader[k], name, MAX_NAME))
				goto foundshader;
		}
		if (k >= MAX_SHADERS)
		{
			printf("ran out of all %i shader slots\n", MAX_SHADERS);
			return 0;
		}
		cleancopyname(shader[k], name, MAX_NAME);
		numshaders++;
foundshader:
		ttris[i][3] = k;
	}

	// remove unused bones
	memset(boneusage, 0, sizeof(boneusage));
	memset(boneremap, 0, sizeof(boneremap));
	for (i = 0;i < numtverts;i++)
		boneusage[tvert[i].bonenum]++;
	// count bones that are referenced as a parent
	for (i = 0;i < numbones;i++)
		if (boneusage[i])
			if (bone[i].parent >= 0)
				boneusage[bone[i].parent]++;
	numtbones = 0;
	for (i = 0;i < numbones;i++)
	{
		if (boneusage[i])
		{
			boneremap[i] = numtbones;
			boneunmap[numtbones] = i;
			cleancopyname(tbone[numtbones].name, bone[i].name, MAX_NAME);
			if (bone[i].parent >= i)
			{
				printf("bone %i's parent (%i) is >= %i\n", i, bone[i].parent, i);
				return 0;
			}
			if (bone[i].parent >= 0)
				tbone[numtbones].parent = boneremap[bone[i].parent];
			else
				tbone[numtbones].parent = -1;
			numtbones++;
		}
		else
			boneremap[i] = -1;
	}
	for (i = 0;i < numtverts;i++)
		tvert[i].bonenum = boneremap[tvert[i].bonenum];

	// build render list
	memset(shaderusage, 0, sizeof(shaderusage));
	memset(shadertris, 0, sizeof(shadertris));
	memset(shadertrisstart, 0, sizeof(shadertrisstart));
	// count shader use
	for (i = 0;i < numtriangles;i++)
		shaderusage[ttris[i][3]]++;
	// prepare lists for triangles
	j = 0;
	for (i = 0;i < numshaders;i++)
	{
		shadertrisstart[i] = j;
		j += shaderusage[i];
	}
	// store triangles into the lists
	for (i = 0;i < numtriangles;i++)
		shadertris[shadertrisstart[ttris[i][3]]++] = i;
	// reset pointers to the start of their lists
	for (i = 0;i < numshaders;i++)
		shadertrisstart[i] -= shaderusage[i];

	mins[0] = mins[1] = mins[2] =  1000000000;
	maxs[0] = maxs[1] = maxs[2] = -1000000000;
	radius = 0;
	for (i = 0;i < numposes;i++)
	{
		int j;
		for (j = 0;j < numbones;j++)
		{
			if (bone[j].parent >= 0)
				concattransform(&bonematrix[bone[j].parent], &pose[i][j], &bonematrix[j]);
			else
				matrixcopy(&pose[i][j], &bonematrix[j]);
		}
		tmins[0] = tmins[1] = tmins[2] =  1000000000;
		tmaxs[0] = tmaxs[1] = tmaxs[2] = -1000000000;
		tradius = 0;
		for (j = 0;j < numtverts;j++)
		{
			transform(tvert[j].origin, &bonematrix[boneunmap[tvert[j].bonenum]], org);
			if (tmins[0] > org[0]) tmins[0] = org[0];if (tmaxs[0] < org[0]) tmaxs[0] = org[0];
			if (tmins[1] > org[1]) tmins[1] = org[1];if (tmaxs[1] < org[1]) tmaxs[1] = org[1];
			if (tmins[2] > org[2]) tmins[2] = org[2];if (tmaxs[2] < org[2]) tmaxs[2] = org[2];
			dist = org[0]*org[0]+org[1]*org[1]+org[2]*org[2];
			if (dist > tradius)
				tradius = dist;
		}
		posemins[i][0] = tmins[0];posemaxs[i][0] = tmaxs[0];
		posemins[i][1] = tmins[1];posemaxs[i][1] = tmaxs[1];
		posemins[i][2] = tmins[2];posemaxs[i][2] = tmaxs[2];
		poseradius[i] = tradius;
		if (mins[0] > tmins[0]) mins[0] = tmins[0];if (maxs[0] < tmaxs[0]) maxs[0] = tmaxs[0];
		if (mins[1] > tmins[1]) mins[1] = tmins[1];if (maxs[1] < tmaxs[1]) maxs[1] = tmaxs[1];
		if (mins[2] > tmins[2]) mins[2] = tmins[2];if (maxs[2] < tmaxs[2]) maxs[2] = tmaxs[2];
		if (radius < tradius) radius = tradius;
	}
	k = 0;
	for (i = 0;i < numscenes;i++)
	{
		tmins[0] = tmins[1] = tmins[2] =  1000000000;
		tmaxs[0] = tmaxs[1] = tmaxs[2] = -1000000000;
		tradius = 0;
		for (j = 0;j < scene[i].length;j++, k++)
		{
			if (tmins[0] > posemins[k][0]) tmins[0] = posemins[k][0];if (tmaxs[0] < posemaxs[k][0]) tmaxs[0] = posemaxs[k][0];
			if (tmins[1] > posemins[k][1]) tmins[1] = posemins[k][1];if (tmaxs[1] < posemaxs[k][1]) tmaxs[1] = posemaxs[k][1];
			if (tmins[2] > posemins[k][2]) tmins[2] = posemins[k][2];if (tmaxs[2] < posemaxs[k][2]) tmaxs[2] = posemaxs[k][2];
			if (tradius < poseradius[k]) tradius = poseradius[k];
		}
		scene[i].mins[0] = tmins[0];scene[i].maxs[0] = tmaxs[0];
		scene[i].mins[1] = tmins[1];scene[i].maxs[1] = tmaxs[1];
		scene[i].mins[2] = tmins[2];scene[i].maxs[2] = tmaxs[2];
		scene[i].radius = tradius;
	}

	putinit();
	putstring("ZYMOTICMODEL", 12);
	putlong(1); // version
	filesizeposition = putgetposition();
	putlong(0); // filesize, will be filled in later
	putfloat(mins[0]);
	putfloat(mins[1]);
	putfloat(mins[2]);
	putfloat(maxs[0]);
	putfloat(maxs[1]);
	putfloat(maxs[2]);
	putfloat(radius);
	putlong(numtverts);
	putlong(numtriangles);
	putlong(numshaders);
	putlong(numtbones);
	putlong(numscenes);
	lumps = putgetposition();
	putnulls(9 * 8); // make room for lumps to be filled in later
	// scenes
	lump[0].start = putgetposition();
	for (i = 0;i < numscenes;i++)
	{
		chopextension(scene[i].name);
		cleancopyname(name, scene[i].name, MAX_NAME);
		putstring(name, MAX_NAME);
		putfloat(scene[i].mins[0]);
		putfloat(scene[i].mins[1]);
		putfloat(scene[i].mins[2]);
		putfloat(scene[i].maxs[0]);
		putfloat(scene[i].maxs[1]);
		putfloat(scene[i].maxs[2]);
		putfloat(scene[i].radius);
		putfloat(scene[i].framerate);
		j = 0;

// normally the scene will loop, if this is set it will stay on the final frame
#define ZYMSCENEFLAG_NOLOOP 1

		if (scene[i].noloop)
			j |= ZYMSCENEFLAG_NOLOOP;
		putlong(j);
		putlong(scene[i].start);
		putlong(scene[i].length);
	}
	lump[0].length = putgetposition() - lump[0].start;
	// poses
	lump[1].start = putgetposition();
	for (i = 0;i < numposes;i++)
	{
		for (j = 0;j < numtbones;j++)
		{
			k = boneunmap[j];
			putfloat(pose[i][k].m[0][0]);
			putfloat(pose[i][k].m[0][1]);
			putfloat(pose[i][k].m[0][2]);
			putfloat(pose[i][k].m[0][3]);
			putfloat(pose[i][k].m[1][0]);
			putfloat(pose[i][k].m[1][1]);
			putfloat(pose[i][k].m[1][2]);
			putfloat(pose[i][k].m[1][3]);
			putfloat(pose[i][k].m[2][0]);
			putfloat(pose[i][k].m[2][1]);
			putfloat(pose[i][k].m[2][2]);
			putfloat(pose[i][k].m[2][3]);
		}
	}
	lump[1].length = putgetposition() - lump[1].start;
	// bones
	lump[2].start = putgetposition();
	for (i = 0;i < numtbones;i++)
	{
		cleancopyname(name, tbone[i].name, MAX_NAME);
		putstring(name, MAX_NAME);
		putlong(0); // bone flags must be set by other utilities
		putlong(tbone[i].parent);
	}
	lump[2].length = putgetposition() - lump[2].start;
	// vertex bone counts
	lump[3].start = putgetposition();
	for (i = 0;i < numtverts;i++)
		putlong(1); // number of bones, always 1 in smd
	lump[3].length = putgetposition() - lump[3].start;
	// vertices
	lump[4].start = putgetposition();
	for (i = 0;i < numtverts;i++)
	{
		// this would be a loop (bonenum origin) if there were more than one bone per vertex (smd file limitation)
		putlong(tvert[i].bonenum);
		// the origin is relative to the bone and scaled by percentage of influence (in smd there is only one bone per vertex, so that would be 1.0)
		putfloat(tvert[i].origin[0]);
		putfloat(tvert[i].origin[1]);
		putfloat(tvert[i].origin[2]);
	}
	lump[4].length = putgetposition() - lump[4].start;
	// texture coordinates for the vertices, separated only for reasons of OpenGL array processing
	lump[5].start = putgetposition();
	for (i = 0;i < numtverts;i++)
	{
		putfloat(tvert[i].texcoord[0]); // s
		putfloat(tvert[i].texcoord[1]); // t
	}
	lump[5].length = putgetposition() - lump[5].start;
	// render list
	lump[6].start = putgetposition();
	// shader numbers are implicit in this list (always sequential), so they are not stored
	for (i = 0;i < numshaders;i++)
	{
		putlong(shaderusage[i]); // how many triangles to follow
		for (j = 0;j < shaderusage[i];j++)
		{
			putlong(ttris[shadertris[shadertrisstart[i]+j]][0]);
			putlong(ttris[shadertris[shadertrisstart[i]+j]][1]);
			putlong(ttris[shadertris[shadertrisstart[i]+j]][2]);
		}
	}
	lump[6].length = putgetposition() - lump[6].start;
	// shader names
	lump[7].start = putgetposition();
	for (i = 0;i < numshaders;i++)
	{
		chopextension(shader[i]);
		cleancopyname(name, shader[i], MAX_NAME);
		putstring(name, MAX_NAME);
	}
	lump[7].length = putgetposition() - lump[7].start;
	// trizone (triangle zone numbers, these must be filled in later by other utilities)
	lump[8].start = putgetposition();
	putnulls(numtriangles);
	lump[8].length = putgetposition() - lump[8].start;
	bufferused = putgetposition();
	putsetposition(lumps);
	for (i = 0;i < 9;i++) // number of lumps
	{
		putlong(lump[i].start);
		putlong(lump[i].length);
	}
	putsetposition(filesizeposition);
	putlong(bufferused);

	// print model stats
	printf("model stats:\n");
	printf("%i vertices %i triangles %i bones %i shaders %i scenes %i poses\n", numtverts, numtriangles, numtbones, numshaders, numscenes, numposes);
	printf("renderlist:\n");
	for (i = 0;i < numshaders;i++)
	{
		for (j = 0;j < MAX_NAME && shader[i][j];j++)
			name[j] = shader[i][j];
		for (;j < MAX_NAME;j++)
			name[j] = ' ';
		name[MAX_NAME] = 0;
		printf("%5i triangles       : %s\n", shaderusage[i], name);
	}
	printf("scenes:\n");
	for (i = 0;i < numscenes;i++)
	{
		for (j = 0;j < MAX_NAME && scene[i].name[j];j++)
			name[j] = scene[i].name[j];
		for (;j < MAX_NAME;j++)
			name[j] = ' ';
		name[MAX_NAME] = 0;
		printf("%5i frames % 3.0f fps %s : %s\n", scene[i].length, scene[i].framerate, scene[i].noloop ? "noloop" : "", name);
	}
	printf("file size: %5ik\n", (bufferused + 512) >> 10);
	writefile(outputname, outputbuffer, bufferused);
//	printf("writing shader and textures\n");
*/
	return 1;
}

