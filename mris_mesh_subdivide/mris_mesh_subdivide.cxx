/**
 * @file  mris_mesh_subdivide.c
 * @brief
 *
 *
 */
/*
 * Original Author: jonathan polimeni
 * CVS Revision Info:
 *    $Author: jonp $
 *    $Date: 2012/09/30 17:41:34 $
 *    $Revision: 1.3 $
 *
 * Copyright © 2011-2012 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


#include <stdio.h>
#include <stdlib.h>

// nint
extern "C"
{
#include "macros.h"
#include "version.h"

#include "error.h"
#include "diag.h"
#include "timer.h"

#include "mri.h"
#include "mrisurf.h"
#include "gcamorph.h"

#include "registerio.h"

#include "resample.h"

// string_to_type
#include "mri_identify.h"
}

#include <vtkVersion.h>
#include <vtkSmartPointer.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkPoints.h>
#include <vtkTriangle.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkSphereSource.h>
#include <vtkButterflySubdivisionFilter.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkLinearSubdivisionFilter.h>

char *Progname = NULL;
static int  parse_commandline(int argc, char **argv);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;

static void argnerr(char *, int) ;

static int debug = 0;

static char vcid[] =
  "$Id: mris_mesh_subdivide.cxx,v 1.3 2012/09/30 17:41:34 jonp Exp $";



int mris_mesh_subdivide__VTK(MRI_SURFACE *mris, int iter);
int mris_mesh_subdivide__convert_mris_VTK(MRI_SURFACE *mris, vtkPolyData *mesh);
int mris_mesh_subdivide__convert_VTK_mris(vtkPolyData *mesh,
    MRI_SURFACE *mris_dst);
int mris_mesh_subdivide__VTK_delete(vtkPolyData *mesh);
int mris_mesh_subdivide__mris_clone_header(MRI_SURFACE *mris_src,
    MRI_SURFACE *mris_dst);

//static char  *subdividemethod_string = "nearest";
static int  subdividemethod = -1;

enum subdividemethod_type
{
  SUBDIVIDE_UNDEFINED = 0,
  SUBDIVIDE_BUTTERFLY = 1,
  SUBDIVIDE_LOOP      = 2,
  SUBDIVIDE_LINEAR    = 3
};

MRI_SURFACE  *mris_subdivide  = NULL;

int iter  = 1;

static char *surf_filename = NULL;
static char *newsurf_filename = NULL;



char *basename (char* path)
{
  char *ptr = strrchr (path, '/');
  return ptr ? ptr + 1 : (char*)path;
}


int main(int argc, char *argv[])
{

  int          nargs = 0;
  int          err;

  MRI_SURFACE  *mris  = NULL;

  int          msec, minutes, seconds ;
  struct timeb start ;

  char cmdline[CMD_LINE_LEN] ;

  make_cmd_version_string
  (argc, argv,
   "$Id: mris_mesh_subdivide.cxx,v 1.3 2012/09/30 17:41:34 jonp Exp $",
   "$Name:  $", cmdline);


  //nargs = handle_version_option (argc, argv,
  //   "$Id: mris_mesh_subdivide.cxx,v 1.3 2012/09/30 17:41:34 jonp Exp $",
  //   "$Name:  $");
  if (nargs && argc - nargs == 1)
  {
    exit (0);
  }
  argc -= nargs;

  Progname = basename(argv[0]) ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  TimerStart(&start) ;

  argc--;
  argv++;

  if (argc == 0)
  {
    usage_exit();
  }

  // set default
  subdividemethod = SUBDIVIDE_BUTTERFLY;

  parse_commandline(argc, argv);


  //==--------------------------------------------------------------
  // 0) read in a surface

  mris = MRISread(surf_filename) ;
  if (!mris)
  {
    ErrorExit(ERROR_NOFILE, "%s: could not read surface %s",
              Progname, surf_filename) ;
  }


  //==--------------------------------------------------------------
  // 1) define subsurface with label or triangle area metric

  // TODO!



  //==--------------------------------------------------------------
  // 2) subdivide!

  // TODO: replace VTK subdivision with our own implementation from
  // scratch

  if ( subdividemethod != -1 )
  {
    err = mris_mesh_subdivide__VTK(mris, iter);
  }


  //==--------------------------------------------------------------
  // 3) check surface quality, self-intersections, non-manifoldness

  // TODO: see mris_warp.c ( mrisurf.c:IsMRISselfIntersecting )

  // TODO: report change in edge length or triangle area over
  // subsurface


  //==--------------------------------------------------------------
  // N) write out surface

  MRISaddCommandLine(mris_subdivide, cmdline);

  printf("writing to %s\n", newsurf_filename);
  MRISwrite(mris_subdivide, newsurf_filename);

  MRISfree(&mris);
  MRISfree(&mris_subdivide);

  msec = TimerStop(&start) ;
  seconds = nint((float)msec/1000.0f) ;
  minutes = seconds / 60 ;
  seconds = seconds % 60 ;
  fprintf(stderr, "%s took %d minutes and %d seconds.\n",
          Progname, minutes, seconds) ;

  exit(0);
  return(0);
}


/* --------------------------------------------------------------------------- */
int mris_mesh_subdivide__VTK(MRI_SURFACE *mris,
                             int iter)
{
  int err;

  vtkPolyData* inputMesh;
  inputMesh = vtkPolyData::New();

  err = mris_mesh_subdivide__convert_mris_VTK(mris, inputMesh);

  std::cout << "Before subdivision" << std::endl;
  std::cout << "    There are " << inputMesh->GetNumberOfPoints()
            << " vertices." << std::endl;
  std::cout << "    There are " << inputMesh->GetNumberOfPolys()
            << " triangle faces." << std::endl;


  // NOTE TO MYSELF [jrp, 2012/sep/29]: in some viewers like freeview
  // the interpolating subdivision does not appear to strictly
  // maintain the positions of original vertices. they are not
  // displaced as much as the approximating algorithms like loop
  // subdivision, but they are clearly not in the same position
  // (whereas the linear subvision vertices are). but this is an
  // artifact of the visualization. i have confirmed that the vertex
  // position values for the original vertices are identical in the
  // interpolating subdivision.


  vtkSmartPointer<vtkPolyDataAlgorithm> subdivisionFilter;

  switch ( subdividemethod )
  {
  case SUBDIVIDE_BUTTERFLY:
    printf("  interpolating subdivision method: 'modified butterfly' \n");
    subdivisionFilter = vtkSmartPointer<vtkButterflySubdivisionFilter>::New();
    dynamic_cast<vtkButterflySubdivisionFilter *>
    (subdivisionFilter.GetPointer())->SetNumberOfSubdivisions(iter);
    break;
  case SUBDIVIDE_LOOP:
    printf("  approximating subdivision method: 'loop' \n");
    subdivisionFilter = vtkSmartPointer<vtkLoopSubdivisionFilter>::New();
    dynamic_cast<vtkLoopSubdivisionFilter *>
    (subdivisionFilter.GetPointer())->SetNumberOfSubdivisions(iter);
    break;
  case SUBDIVIDE_LINEAR:
    printf("  interpolating subdivision method: 'linear' \n");
    subdivisionFilter = vtkSmartPointer<vtkLinearSubdivisionFilter>::New();
    dynamic_cast<vtkLinearSubdivisionFilter *>
    (subdivisionFilter.GetPointer())->SetNumberOfSubdivisions(iter);
    break;
  case -1:
    printf("method uninitialized\n");
    break;
  default:
    printf("this should not happen\n");
    break;
  }

#if VTK_MAJOR_VERSION <= 5
  subdivisionFilter->SetInputConnection(inputMesh->GetProducerPort());
#else
  subdivisionFilter->SetInputData(inputMesh);
#endif


  printf("surface subdividision iterations: %d\n", iter);
  subdivisionFilter->Update();

  vtkPolyData* outputMesh;
  outputMesh = subdivisionFilter->GetOutput();


  std::cout << "After subdivision" << std::endl;
  std::cout << "    There are " << outputMesh->GetNumberOfPoints()
            << " vertices." << std::endl;
  std::cout << "    There are " << outputMesh->GetNumberOfPolys()
            << " triangle faces." << std::endl;


  mris_subdivide = MRISalloc(outputMesh->GetNumberOfPoints(),
                             outputMesh->GetNumberOfPolys()) ;
  err = mris_mesh_subdivide__mris_clone_header(mris, mris_subdivide);
  err = mris_mesh_subdivide__convert_VTK_mris(outputMesh, mris_subdivide);

  err = mris_mesh_subdivide__VTK_delete(inputMesh);
  //  err = mris_mesh_subdivide__VTK_delete(outputMesh);

  return(0);
}


/* --------------------------------------------------------------------------- */
int mris_mesh_subdivide__convert_mris_VTK(MRI_SURFACE *mris,
    vtkPolyData *mesh)
{
  vtkPoints* points;
  vtkCellArray* faces;

  points = vtkPoints::New();
  points->SetNumberOfPoints( mris->nvertices );
  VERTEX* pvtx = &( mris->vertices[0] );
  for (unsigned int ui(0), nvertices(mris->nvertices);
       ui < nvertices; ++ui, ++pvtx )
  {
    points->SetPoint(ui, pvtx->x, pvtx->y, pvtx->z);
  } // next point (vertex)

  faces = vtkCellArray::New();
  faces->Allocate( mris->nfaces );
  FACE* pface = &( mris->faces[0] );
  for ( unsigned int ui(0), nfaces(mris->nfaces);
        ui < nfaces; ++ui, ++pface )
  {
    faces->InsertNextCell((long long int)VERTICES_PER_FACE);
    for ( int i = 0; i < VERTICES_PER_FACE; i++ )
    {
      faces->InsertCellPoint(pface->v[i]);
    }
  } // next cell (face)

  mesh->SetPoints(points);
  mesh->SetPolys(faces);
  mesh->Modified();

  return 0;
}


/* --------------------------------------------------------------------------- */
int mris_mesh_subdivide__convert_VTK_mris(vtkPolyData *mesh,
    MRI_SURFACE *mris_dst)
{

  int      vno, fno;
  unsigned int uvno;
  VERTEX   *v ;
  double p[3];

  vtkSmartPointer<vtkIdList> pointIdList =
    vtkSmartPointer<vtkIdList>::New();

  for (uvno = 0, vno = 0 ; vno < mris_dst->nvertices ; uvno++, vno++)
  {
    v = &mris_dst->vertices[vno] ;
    mesh->GetPoint(uvno, p);

    v->x = p[0];
    v->y = p[1];
    v->z = p[2];

  }

  for (fno = 0; fno < mris_dst->nfaces; fno++)
  {
    mesh->GetCellPoints(fno, pointIdList);

    mris_dst->faces[fno].v[0] = pointIdList->GetId(0);
    mris_dst->faces[fno].v[1] = pointIdList->GetId(1);
    mris_dst->faces[fno].v[2] = pointIdList->GetId(2);
  }


  return 0;
}


/* --------------------------------------------------------------------------- */
int mris_mesh_subdivide__mris_clone_header(MRI_SURFACE *mris_src,
    MRI_SURFACE *mris_dst)
{
  int ind;

  mris_dst->type = mris_src->type;

  mris_dst->hemisphere = mris_src->hemisphere ;
  mris_dst->xctr = mris_src->xctr ;
  mris_dst->yctr = mris_src->yctr ;
  mris_dst->zctr = mris_src->zctr ;
  mris_dst->xlo = mris_src->xlo ;
  mris_dst->ylo = mris_src->ylo ;
  mris_dst->zlo = mris_src->zlo ;
  mris_dst->xhi = mris_src->xhi ;
  mris_dst->yhi = mris_src->yhi ;
  mris_dst->zhi = mris_src->zhi ;


  mris_dst->lta = mris_src->lta;
  mris_dst->SRASToTalSRAS_ = mris_src->SRASToTalSRAS_;
  mris_dst->TalSRASToSRAS_ = mris_src->TalSRASToSRAS_;
  mris_dst->free_transform = 0 ;

  mris_dst->useRealRAS = mris_src->useRealRAS;
  copyVolGeom(&mris_src->vg, &mris_dst->vg);

  mris_dst->group_avg_surface_area = mris_src->group_avg_surface_area;
  {
    mris_dst->ncmds = mris_src->ncmds;
    for (ind = 0 ; ind < mris_src->ncmds ; ind++)
    {
      mris_dst->cmdlines[ind] =
        (char *)calloc(strlen(mris_src->cmdlines[ind])+1, sizeof(char)) ;
      strcpy(mris_dst->cmdlines[ind], mris_src->cmdlines[ind]);
    }
  }

  return 0;
}


/* --------------------------------------------------------------------------- */
int mris_mesh_subdivide__VTK_delete(vtkPolyData *mesh)
{
  vtkPoints* points;
  vtkCellArray* faces;

  points = mesh->GetPoints();
  faces = mesh->GetPolys();

  mesh->Delete();
  faces->Delete();
  points->Delete();

  return 0;
}


/* --------------------------------------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;
  char *tmpstr = NULL;

  if (argc < 1)
  {
    usage_exit();
  }

  nargc = argc;
  pargv = argv;
  while (nargc > 0)
  {
    option = pargv[0];
    if (debug)
    {
      printf("%d %s\n",nargc,option);
    }
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))
    {
      print_help() ;
    }
    else if (!strcasecmp(option, "--version"))
    {
      print_version() ;
    }
    else if (!strcasecmp(option, "--debug"))
    {
      debug = 1;
    }

    /*--------------*/

    else if (!strcmp(option, "--surf"))
    {
      if (nargc < 1)
      {
        argnerr(option,1);
      }
      surf_filename = pargv[0];
      if( !strcmp(surf_filename, "inflated") )
      {
        printf("\nWARNING: do you really want to subdivide the "
               "*inflated* surface?\n\n");
        exit(1);
      }
      nargsused = 1;
    }
    else if (!strcasecmp(option, "--out"))
    {
      if (nargc < 1)
      {
        argnerr(option,1);
      }
      newsurf_filename = pargv[0];
      nargsused = 1;
    }
    else if ( !strcmp(option, "--iter") )
    {
      if (nargc < 1)
      {
        argnerr(option,1);
      }
      iter = atoi(pargv[0]);
      nargsused = 1;
    }
    else if ( !strcmp(option, "--method") )
    {
      if (nargc < 1)
      {
        argnerr(option,1);
      }
      tmpstr = pargv[0];
      nargsused = 1;

      if ( !tmpstr )
      {
        printf("option 'method' requires argument\n");
      }
      else if ( !strcmp(tmpstr, "butterfly") )
      {
        subdividemethod = SUBDIVIDE_BUTTERFLY;
      }
      else if ( !strcmp(tmpstr, "loop") )
      {
        subdividemethod = SUBDIVIDE_LOOP;
      }
      else if ( !strcmp(tmpstr, "linear") )
      {
        subdividemethod = SUBDIVIDE_LINEAR;
      }
      else
      {
        subdividemethod = SUBDIVIDE_UNDEFINED;
        ErrorExit(ERROR_NOFILE, "%s: method '%s' unrecognized, see help",
                  Progname, tmpstr) ;

      }
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}

/* --------------------------------------------------------------------------- */
static void argnerr(char *option, int n)
{
  if (n==1)
  {
    fprintf(stderr,"ERROR: %s flag needs %d argument\n",option,n);
  }
  else
  {
    fprintf(stderr,"ERROR: %s flag needs %d arguments\n",option,n);
  }
  exit(-1) ;
}


/* --------------------------------------------------------------------------- */
static void usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}

/* --------------------------------------------------------------------------- */
static void print_usage(void)
{
  printf("USAGE: %s  [options]\n",Progname) ;
  printf("\n");
  printf("   --surf <filename>       name of input surface\n");
  printf("   --out  <filename>       name for output surface (if does not\n");
  printf("                           contain '/' outputs to same directory\n");
  printf("                           as input surface\n");
  printf("   --method <methodname>   subdivision method options are:\n");
  printf("                           'butterfly', 'loop', or 'linear'\n");
  printf("   --iter <N>              number of subdivision iterations'\n");
  printf("\n");
  printf("\n");
  printf("   --help        print out information on how to use this program\n");
  printf("   --version     print out version and exit\n");
  printf("\n");
  printf("%s\n", vcid) ;
  printf("\n");
}

/* --------------------------------------------------------------------------- */
static void print_help(void)
{
  print_usage() ;

  printf(
    "This program will subdivide a surface.\n"
  ) ;

  exit(1) ;
}

/* --------------------------------------------------------------------------- */
ystatic void print_version(void)
{
  printf("%s\n", vcid) ;
  exit(1) ;
}