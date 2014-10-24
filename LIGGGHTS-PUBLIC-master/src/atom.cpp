/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "mpi.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "limits.h"
#include "atom.h"
#include "style_atom.h"
#include "atom_vec.h"
#include "atom_vec_ellipsoid.h"
#include "comm.h"
#include "neighbor.h"
#include "force.h"
#include "modify.h"
#include "fix.h"
#include "output.h"
#include "thermo.h"
#include "update.h"
#include "domain.h"
#include "group.h"
#include "accelerator_cuda.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

#define DELTA 1
#define DELTA_MEMSTR 1024
#define EPSILON 1.0e-6
#define CUDA_CHUNK 3000

/* ---------------------------------------------------------------------- */

Atom::Atom(LAMMPS *lmp) : Pointers(lmp)
{
  natoms = 0;
  nlocal = nghost = nmax = 0;
  ntypes = 0;
  nbondtypes = nangletypes = ndihedraltypes = nimpropertypes = 0;
  nbonds = nangles = ndihedrals = nimpropers = 0;
  bond_per_atom = angle_per_atom = dihedral_per_atom = improper_per_atom = 0;
  extra_bond_per_atom = 0;

  firstgroupname = NULL;
  sortfreq = 1000;
  nextsort = 0;
  userbinsize = 0.0;
  maxbin = maxnext = 0;
  binhead = NULL;
  next = permute = NULL;

  // initialize atom arrays
  // customize by adding new array

  tag = type = mask = image = NULL;
  x = v = f = NULL;

  molecule = NULL;
  q = NULL;
  mu = NULL;
  omega = angmom = torque = NULL;
  radius = rmass = NULL;
  density = NULL; 
  vfrac = s0 = NULL;
  x0 = NULL;
  ellipsoid = line = tri = NULL;
  spin = NULL;
  eradius = ervel = erforce = NULL;
  cs = csforce = vforce = ervelforce = NULL;
  etag = NULL;
  rho = drho = NULL;
  p = NULL; 
  e = de = NULL;
  cv = NULL;
  vest = NULL;

  maxspecial = 1;
  nspecial = NULL;
  special = NULL;

  num_bond = NULL;
  bond_type = bond_atom = NULL;
  bond_hist = NULL;

  num_angle = NULL;
  angle_type = angle_atom1 = angle_atom2 = angle_atom3 = NULL;

  num_dihedral = NULL;
  dihedral_type = dihedral_atom1 = dihedral_atom2 = NULL;
  dihedral_atom3 = dihedral_atom4 = NULL;

  num_improper = NULL;
  improper_type = improper_atom1 = improper_atom2 = NULL;
  improper_atom3 = improper_atom4 = NULL;

  // initialize atom style and array existence flags
  // customize by adding new flag

  sphere_flag = ellipsoid_flag = line_flag = tri_flag = 0;
  peri_flag = electron_flag = 0;
  wavepacket_flag = sph_flag = 0;

  molecule_flag = q_flag = mu_flag = 0;
  rmass_flag = radius_flag = omega_flag = torque_flag = angmom_flag = 0;
  density_flag = NULL; 
  vfrac_flag = spin_flag = eradius_flag = ervel_flag = erforce_flag = 0;
  cs_flag = csforce_flag = vforce_flag = ervelforce_flag= etag_flag = 0;
  rho_flag = e_flag = cv_flag = vest_flag = 0;
  p_flag = 0; 

  // ntype-length arrays

  mass = NULL;
  mass_setflag = NULL;

  // callback lists & extra restart info

  nextra_grow = nextra_restart = 0;
  extra_grow = extra_restart = NULL;
  nextra_grow_max = nextra_restart_max = 0;
  nextra_store = 0;
  extra = NULL;

  // default mapping values and hash table primes

  tag_enable = 1;
  map_style = 0;
  map_tag_max = 0;
  map_nhash = 0;

  nprimes = 38;
  primes = new int[nprimes];
  int plist[] = {5041,10007,20011,30011,40009,50021,60013,70001,80021,
                 90001,100003,110017,120011,130003,140009,150001,160001,
                 170003,180001,190027,200003,210011,220009,230003,240007,
                 250007,260003,270001,280001,290011,300007,310019,320009,
                 330017,340007,350003,362881,3628801};
  for (int i = 0; i < nprimes; i++) primes[i] = plist[i];

  // default atom style = atomic

  atom_style = NULL;
  avec = NULL;
  create_avec("atomic",0,NULL,lmp->suffix);

  radvary_flag = 0;
}

/* ---------------------------------------------------------------------- */

Atom::~Atom()
{
  delete [] atom_style;
  delete avec;

  delete [] firstgroupname;
  memory->destroy(binhead);
  memory->destroy(next);
  memory->destroy(permute);

  // delete atom arrays
  // customize by adding new array

  memory->destroy(tag);
  memory->destroy(type);
  memory->destroy(mask);
  memory->destroy(image);
  memory->destroy(x);
  memory->destroy(v);
  memory->destroy(f);

  memory->destroy(q);
  memory->destroy(mu);
  memory->destroy(omega);
  memory->destroy(angmom);
  memory->destroy(torque);
  memory->destroy(radius);
  memory->destroy(rmass);
  memory->destroy(density); 
  memory->destroy(vfrac);
  memory->destroy(s0);
  memory->destroy(x0);
  memory->destroy(ellipsoid);
  memory->destroy(line);
  memory->destroy(tri);
  memory->destroy(spin);
  memory->destroy(eradius);
  memory->destroy(ervel);
  memory->destroy(erforce);

  memory->destroy(rho);
  memory->destroy(drho);
  memory->destroy(e);
  memory->destroy(de);
  memory->destroy(p);

  memory->destroy(molecule);

  memory->destroy(nspecial);
  memory->destroy(special);

  memory->destroy(num_bond);
  memory->destroy(bond_type);
  memory->destroy(bond_atom);
  memory->destroy(bond_hist); 

  memory->destroy(num_angle);
  memory->destroy(angle_type);
  memory->destroy(angle_atom1);
  memory->destroy(angle_atom2);
  memory->destroy(angle_atom3);

  memory->destroy(num_dihedral);
  memory->destroy(dihedral_type);
  memory->destroy(dihedral_atom1);
  memory->destroy(dihedral_atom2);
  memory->destroy(dihedral_atom3);
  memory->destroy(dihedral_atom4);

  memory->destroy(num_improper);
  memory->destroy(improper_type);
  memory->destroy(improper_atom1);
  memory->destroy(improper_atom2);
  memory->destroy(improper_atom3);
  memory->destroy(improper_atom4);

  // delete per-type arrays

  delete [] mass;
  delete [] mass_setflag;

  // delete extra arrays

  memory->destroy(extra_grow);
  memory->destroy(extra_restart);
  memory->destroy(extra);

  // delete mapping data structures

  map_delete();
  delete [] primes;
}

/* ----------------------------------------------------------------------
   copy modify settings from old Atom class to current Atom class
------------------------------------------------------------------------- */

void Atom::settings(Atom *old)
{
  map_style = old->map_style;
}

/* ----------------------------------------------------------------------
   create an AtomVec style
   called from input script, restart file, replicate
------------------------------------------------------------------------- */

void Atom::create_avec(const char *style, int narg, char **arg, char *suffix)
{
  delete [] atom_style;
  if (avec) delete avec;

  // unset atom style and array existence flags
  // may have been set by old avec
  // customize by adding new flag

  sphere_flag = ellipsoid_flag = line_flag = tri_flag = 0;
  peri_flag = electron_flag = 0;

  molecule_flag = q_flag = mu_flag = 0;
  rmass_flag = radius_flag = omega_flag = torque_flag = angmom_flag = 0;
  density_flag = 0; 
  rho_flag = p_flag = 0; 
  vfrac_flag = spin_flag = eradius_flag = ervel_flag = erforce_flag = 0;

  int sflag;
  avec = new_avec(style,narg,arg,suffix,sflag);

  if (sflag) {
    char estyle[256];
    sprintf(estyle,"%s/%s",style,suffix);
    int n = strlen(estyle) + 1;
    atom_style = new char[n];
    strcpy(atom_style,estyle);
  } else {
    int n = strlen(style) + 1;
    atom_style = new char[n];
    strcpy(atom_style,style);
  }

  // if molecular system, default is to have array map

  molecular = avec->molecular;
  if (map_style == 0 && molecular) map_style = 1;
}

/* ----------------------------------------------------------------------
   generate an AtomVec class, first with suffix appended
------------------------------------------------------------------------- */

AtomVec *Atom::new_avec(const char *style, int narg, char **arg,
                        char *suffix, int &sflag)
{
  if (suffix && lmp->suffix_enable) {
    sflag = 1;
    char estyle[256];
    sprintf(estyle,"%s/%s",style,suffix);

    if (0) return NULL;

#define ATOM_CLASS
#define AtomStyle(key,Class) \
    else if (strcmp(estyle,#key) == 0) return new Class(lmp,narg,arg);
#include "style_atom.h"
#undef AtomStyle
#undef ATOM_CLASS

  }

  sflag = 0;

  if (0) return NULL;

#define ATOM_CLASS
#define AtomStyle(key,Class) \
  else if (strcmp(style,#key) == 0) return new Class(lmp,narg,arg);
#include "style_atom.h"
#undef ATOM_CLASS

  else error->all(FLERR,"Invalid atom style");

  return NULL;
}

/* ---------------------------------------------------------------------- */

void Atom::init()
{
  // delete extra array since it doesn't persist past first run

  if (nextra_store) {
    memory->destroy(extra);
    extra = NULL;
    nextra_store = 0;
  }

  // check arrays that are atom type in length

  check_mass();

  // setup of firstgroup

  if (firstgroupname) {
    firstgroup = group->find(firstgroupname);
    if (firstgroup < 0)
      error->all(FLERR,"Could not find atom_modify first group ID");
  } else firstgroup = -1;

  // init AtomVec

  avec->init();
}

/* ---------------------------------------------------------------------- */

void Atom::setup()
{
  // setup bins for sorting
  // cannot do this in init() because uses neighbor cutoff

  if (sortfreq > 0) setup_sort_bins();
}

/* ----------------------------------------------------------------------
   return ptr to AtomVec class if matches style or to matching hybrid sub-class
   return NULL if no match
------------------------------------------------------------------------- */

AtomVec *Atom::style_match(const char *style)
{
  if (strcmp(atom_style,style) == 0) return avec;
  else if (strcmp(atom_style,"hybrid") == 0) {
    AtomVecHybrid *avec_hybrid = (AtomVecHybrid *) avec;
    for (int i = 0; i < avec_hybrid->nstyles; i++)
      if (strcmp(avec_hybrid->keywords[i],style) == 0)
        return avec_hybrid->styles[i];
  }
  return NULL;
}

/* ----------------------------------------------------------------------
   modify parameters of the atom style
   some options can only be invoked before simulation box is defined
   first and sort options cannot be used together
------------------------------------------------------------------------- */

void Atom::modify_params(int narg, char **arg)
{
  if (narg == 0) error->all(FLERR,"Illegal atom_modify command");

  int iarg = 0;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"map") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal atom_modify command");
      if (strcmp(arg[iarg+1],"array") == 0) map_style = 1;
      else if (strcmp(arg[iarg+1],"hash") == 0) map_style = 2;
      else error->all(FLERR,"Illegal atom_modify command");
      if (domain->box_exist)
        error->all(FLERR,"Atom_modify map command after simulation box is defined");
      iarg += 2;
    } else if (strcmp(arg[iarg],"first") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal atom_modify command");
      if (strcmp(arg[iarg+1],"all") == 0) {
        delete [] firstgroupname;
        firstgroupname = NULL;
      } else {
        int n = strlen(arg[iarg+1]) + 1;
        firstgroupname = new char[n];
        strcpy(firstgroupname,arg[iarg+1]);
        sortfreq = 0;
      }
      iarg += 2;
    } else if (strcmp(arg[iarg],"sort") == 0) {
      if (iarg+3 > narg) error->all(FLERR,"Illegal atom_modify command");
      sortfreq = atoi(arg[iarg+1]);
      userbinsize = atof(arg[iarg+2]);
      if (sortfreq < 0 || userbinsize < 0.0)
        error->all(FLERR,"Illegal atom_modify command");
      if (sortfreq >= 0 && firstgroupname)
        error->all(FLERR,"Atom_modify sort and first options "
                   "cannot be used together");
      iarg += 3;
    } else error->all(FLERR,"Illegal atom_modify command");
  }
}

/* ----------------------------------------------------------------------
   allocate and initialize array or hash table for global -> local map
   set map_tag_max = largest atom ID (may be larger than natoms)
   for array option:
     array length = 1 to largest tag of any atom
     set entire array to -1 as initial values
   for hash option:
     map_nhash = length of hash table
     map_nbucket = # of hash buckets, prime larger than map_nhash
       so buckets will only be filled with 0 or 1 atoms on average
------------------------------------------------------------------------- */

void Atom::map_init()
{
  map_delete();

  if (tag_enable == 0)
    error->all(FLERR,"Cannot create an atom map unless atoms have IDs");

  int max = 0;
  for (int i = 0; i < nlocal; i++) max = MAX(max,tag[i]);
  MPI_Allreduce(&max,&map_tag_max,1,MPI_INT,MPI_MAX,world);

  if (map_style == 1) {
    memory->create(map_array,map_tag_max+1,"atom:map_array");
    for (int i = 0; i <= map_tag_max; i++) map_array[i] = -1;

  } else {

    // map_nhash = max of atoms/proc or total atoms, times 2, at least 1000

    int nper = static_cast<int> (natoms/comm->nprocs);
    map_nhash = MAX(nper,nmax);
    if (map_nhash > natoms) map_nhash = static_cast<int> (natoms);
    if (comm->nprocs > 1) map_nhash *= 2;
    map_nhash = MAX(map_nhash,1000);

    // map_nbucket = prime just larger than map_nhash

    int n = map_nhash/10000;
    n = MIN(n,nprimes-1);
    map_nbucket = primes[n];
    if (map_nbucket < map_nhash && n < nprimes-1) map_nbucket = primes[n+1];

    // set all buckets to empty
    // set hash to map_nhash in length
    // put all hash entries in free list and point them to each other

    map_bucket = new int[map_nbucket];
    for (int i = 0; i < map_nbucket; i++) map_bucket[i] = -1;

    map_hash = new HashElem[map_nhash];
    map_nused = 0;
    map_free = 0;
    for (int i = 0; i < map_nhash; i++) map_hash[i].next = i+1;
    map_hash[map_nhash-1].next = -1;
  }
}

/* ----------------------------------------------------------------------
   clear global -> local map for all of my own and ghost atoms
   for hash table option:
     global ID may not be in table if image atom was already cleared
------------------------------------------------------------------------- */

void Atom::map_clear()
{
  if (map_style == 1) {
    int nall = nlocal + nghost;
    for (int i = 0; i < nall; i++) map_array[tag[i]] = -1;

  } else {
    int previous,global,ibucket,index;
    int nall = nlocal + nghost;
    for (int i = 0; i < nall; i++) {

      // search for key
      // if don't find it, done

      previous = -1;
      global = tag[i];
      ibucket = global % map_nbucket;
      index = map_bucket[ibucket];
      while (index > -1) {
        if (map_hash[index].global == global) break;
        previous = index;
        index = map_hash[index].next;
      }
      if (index == -1) continue;

      // delete the hash entry and add it to free list
      // special logic if entry is 1st in the bucket

      if (previous == -1) map_bucket[ibucket] = map_hash[index].next;
      else map_hash[previous].next = map_hash[index].next;

      map_hash[index].next = map_free;
      map_free = index;
      map_nused--;
    }
  }
}

/* ----------------------------------------------------------------------
   set global -> local map for all of my own and ghost atoms
   loop in reverse order so that nearby images take precedence over far ones
     and owned atoms take precedence over images
   this enables valid lookups of bond topology atoms
   for hash table option:
     if hash table too small, re-init
     global ID may already be in table if image atom was set
------------------------------------------------------------------------- */

void Atom::map_set()
{
  if (map_style == 1) {
    int nall = nlocal + nghost;
    for (int i = nall-1; i >= 0 ; i--) map_array[tag[i]] = i;

  } else {
    int previous,global,ibucket,index;
    int nall = nlocal + nghost;
    if (nall > map_nhash) map_init();

    for (int i = nall-1; i >= 0 ; i--) {

      // search for key
      // if found it, just overwrite local value with index

      previous = -1;
      global = tag[i];
      ibucket = global % map_nbucket;
      index = map_bucket[ibucket];
      while (index > -1) {
        if (map_hash[index].global == global) break;
        previous = index;
        index = map_hash[index].next;
      }
      if (index > -1) {
        map_hash[index].local = i;
        continue;
      }

      // take one entry from free list
      // add the new global/local pair as entry at end of bucket list
      // special logic if this entry is 1st in bucket

      index = map_free;
      map_free = map_hash[map_free].next;
      if (previous == -1) map_bucket[ibucket] = index;
      else map_hash[previous].next = index;
      map_hash[index].global = global;
      map_hash[index].local = i;
      map_hash[index].next = -1;
      map_nused++;
    }
  }
}

/* ----------------------------------------------------------------------
   set global to local map for one atom
   for hash table option:
     global ID may already be in table if atom was already set
------------------------------------------------------------------------- */

void Atom::map_one(int global, int local)
{
  if (map_style == 1) map_array[global] = local;

  else {
    // search for key
    // if found it, just overwrite local value with index

    int previous = -1;
    int ibucket = global % map_nbucket;
    int index = map_bucket[ibucket];
    while (index > -1) {
      if (map_hash[index].global == global) break;
      previous = index;
      index = map_hash[index].next;
    }
    if (index > -1) {
      map_hash[index].local = local;
      return;
    }

    // take one entry from free list
    // add the new global/local pair as entry at end of bucket list
    // special logic if this entry is 1st in bucket

    index = map_free;
    map_free = map_hash[map_free].next;
    if (previous == -1) map_bucket[ibucket] = index;
    else map_hash[previous].next = index;
    map_hash[index].global = global;
    map_hash[index].local = local;
    map_hash[index].next = -1;
    map_nused++;
  }
}

/* ----------------------------------------------------------------------
   free the array or hash table for global to local mapping
------------------------------------------------------------------------- */

void Atom::map_delete()
{
  if (map_style == 1) {
    if (map_tag_max) memory->destroy(map_array);
  } else {
    if (map_nhash) {
      delete [] map_bucket;
      delete [] map_hash;
    }
    map_nhash = 0;
  }
  map_tag_max = 0;
}

/* ----------------------------------------------------------------------
   lookup global ID in hash table, return local index
------------------------------------------------------------------------- */

int Atom::map_find_hash(int global)
{
  int local = -1;
  int index = map_bucket[global % map_nbucket];
  while (index > -1) {
    if (map_hash[index].global == global) {
      local = map_hash[index].local;
      break;
    }
    index = map_hash[index].next;
  }
  return local;
}

/* ----------------------------------------------------------------------
   add unique tags to any atoms with tag = 0
   new tags are grouped by proc and start after max current tag
   called after creating new atoms
------------------------------------------------------------------------- */

void Atom::tag_extend()
{
  // maxtag_all = max tag for all atoms

  int maxtag = 0;
  for (int i = 0; i < nlocal; i++) maxtag = MAX(maxtag,tag[i]);
  int maxtag_all;
  MPI_Allreduce(&maxtag,&maxtag_all,1,MPI_INT,MPI_MAX,world);

  // notag = # of atoms I own with no tag (tag = 0)
  // notag_sum = # of total atoms on procs <= me with no tag

  int notag = 0;
  for (int i = 0; i < nlocal; i++) if (tag[i] == 0) notag++;
  int notag_sum;
  MPI_Scan(&notag,&notag_sum,1,MPI_INT,MPI_SUM,world);

  // itag = 1st new tag that my untagged atoms should use

  int itag = maxtag_all + notag_sum - notag + 1;
  for (int i = 0; i < nlocal; i++) if (tag[i] == 0) tag[i] = itag++;
}

/* ----------------------------------------------------------------------
   check that atom IDs span range from 1 to Natoms
   return 0 if mintag != 1 or maxtag != Natoms
   return 1 if OK
   doesn't actually check if all tag values are used
------------------------------------------------------------------------- */

int Atom::tag_consecutive()
{
  int idmin = MAXTAGINT;
  int idmax = 0;

  for (int i = 0; i < nlocal; i++) {
    idmin = MIN(idmin,tag[i]);
    idmax = MAX(idmax,tag[i]);
  }
  int idminall,idmaxall;
  MPI_Allreduce(&idmin,&idminall,1,MPI_INT,MPI_MIN,world);
  MPI_Allreduce(&idmax,&idmaxall,1,MPI_INT,MPI_MAX,world);

  if (idminall != 1 || idmaxall != static_cast<int> (natoms)) return 0;
  return 1;
}

/* ----------------------------------------------------------------------
   get max tag
------------------------------------------------------------------------- */

bigint Atom::tag_max()
{
  bigint idmax = 0;

  for (int i = 0; i < nlocal; i++) {
    idmax = MAX(idmax,tag[i]);
  }
  int idmaxall;
  MPI_Allreduce(&idmax,&idmaxall,1,MPI_INT,MPI_MAX,world);
  return idmaxall;
}

/* ----------------------------------------------------------------------
   count and return words in a single line
   make copy of line before using strtok so as not to change line
   trim anything from '#' onward
------------------------------------------------------------------------- */

int Atom::count_words(const char *line)
{
  int n = strlen(line) + 1;
  char *copy;
  memory->create(copy,n,"atom:copy");
  strcpy(copy,line);

  char *ptr;
  if (ptr = strchr(copy,'#')) *ptr = '\0';

  if (strtok(copy," \t\n\r\f") == NULL) {
    memory->destroy(copy);
    return 0;
  }
  n = 1;
  while (strtok(NULL," \t\n\r\f")) n++;

  memory->destroy(copy);
  return n;
}

/* ----------------------------------------------------------------------
   unpack n lines from Atom section of data file
   call style-specific routine to parse line
------------------------------------------------------------------------- */

void Atom::data_atoms(int n, char *buf)
{
  int m,imagedata,xptr,iptr;
  double xdata[3],lamda[3];
  double *coord;
  char *next;

  next = strchr(buf,'\n');
  *next = '\0';
  int nwords = count_words(buf);
  *next = '\n';

  if (nwords != avec->size_data_atom && nwords != avec->size_data_atom + 3)
    error->all(FLERR,"Incorrect atom format in data file");

  char **values = new char*[nwords];

  // set bounds for my proc
  // if periodic and I am lo/hi proc, adjust bounds by EPSILON
  // insures all data atoms will be owned even with round-off

  int triclinic = domain->triclinic;

  double epsilon[3];
  if (triclinic) epsilon[0] = epsilon[1] = epsilon[2] = EPSILON;
  else {
    epsilon[0] = domain->prd[0] * EPSILON;
    epsilon[1] = domain->prd[1] * EPSILON;
    epsilon[2] = domain->prd[2] * EPSILON;
  }

  double sublo[3],subhi[3];
  if (triclinic == 0) {
    sublo[0] = domain->sublo[0]; subhi[0] = domain->subhi[0];
    sublo[1] = domain->sublo[1]; subhi[1] = domain->subhi[1];
    sublo[2] = domain->sublo[2]; subhi[2] = domain->subhi[2];
  } else {
    sublo[0] = domain->sublo_lamda[0]; subhi[0] = domain->subhi_lamda[0];
    sublo[1] = domain->sublo_lamda[1]; subhi[1] = domain->subhi_lamda[1];
    sublo[2] = domain->sublo_lamda[2]; subhi[2] = domain->subhi_lamda[2];
  }

  if (domain->xperiodic) {
    if (comm->myloc[0] == 0) sublo[0] -= epsilon[0];
    if (comm->myloc[0] == comm->procgrid[0]-1) subhi[0] += epsilon[0];
  }
  if (domain->yperiodic) {
    if (comm->myloc[1] == 0) sublo[1] -= epsilon[1];
    if (comm->myloc[1] == comm->procgrid[1]-1) subhi[1] += epsilon[1];
  }
  if (domain->zperiodic) {
    if (comm->myloc[2] == 0) sublo[2] -= epsilon[2];
    if (comm->myloc[2] == comm->procgrid[2]-1) subhi[2] += epsilon[2];
  }

  // xptr = which word in line starts xyz coords
  // iptr = which word in line starts ix,iy,iz image flags

  xptr = avec->xcol_data - 1;
  int imageflag = 0;
  if (nwords > avec->size_data_atom) imageflag = 1;
  if (imageflag) iptr = nwords - 3;

  // loop over lines of atom data
  // tokenize the line into values
  // extract xyz coords and image flags
  // remap atom into simulation box
  // if atom is in my sub-domain, unpack its values

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');

    values[0] = strtok(buf," \t\n\r\f");
    if (values[0] == NULL)
      error->all(FLERR,"Incorrect atom format in data file");
    for (m = 1; m < nwords; m++) {
      values[m] = strtok(NULL," \t\n\r\f");
      if (values[m] == NULL)
        error->all(FLERR,"Incorrect atom format in data file");
    }

    if (imageflag)
      imagedata = ((atoi(values[iptr+2]) + 512 & 1023) << 20) |
        ((atoi(values[iptr+1]) + 512 & 1023) << 10) |
        (atoi(values[iptr]) + 512 & 1023);
    else imagedata = (512 << 20) | (512 << 10) | 512;

    xdata[0] = atof(values[xptr]);
    xdata[1] = atof(values[xptr+1]);
    xdata[2] = atof(values[xptr+2]);
    domain->remap(xdata,imagedata);
    if (triclinic) {
      domain->x2lamda(xdata,lamda);
      coord = lamda;
    } else coord = xdata;

    if (coord[0] >= sublo[0] && coord[0] < subhi[0] &&
        coord[1] >= sublo[1] && coord[1] < subhi[1] &&
        coord[2] >= sublo[2] && coord[2] < subhi[2])
      avec->data_atom(xdata,imagedata,values);

    buf = next + 1;
  }

  delete [] values;
}

/* ----------------------------------------------------------------------
   unpack n lines from Velocity section of data file
   check that atom IDs are > 0 and <= map_tag_max
   call style-specific routine to parse line
------------------------------------------------------------------------- */

void Atom::data_vels(int n, char *buf)
{
  int j,m,tagdata;
  char *next;

  next = strchr(buf,'\n');
  *next = '\0';
  int nwords = count_words(buf);
  *next = '\n';

  if (nwords != avec->size_data_vel)
    error->all(FLERR,"Incorrect velocity format in data file");

  char **values = new char*[nwords];

  // loop over lines of atom velocities
  // tokenize the line into values
  // if I own atom tag, unpack its values

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');

    values[0] = strtok(buf," \t\n\r\f");
    for (j = 1; j < nwords; j++)
      values[j] = strtok(NULL," \t\n\r\f");

    tagdata = atoi(values[0]);
    if (tagdata <= 0 || tagdata > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Velocities section of data file");
    if ((m = map(tagdata)) >= 0) avec->data_vel(m,&values[1]);

    buf = next + 1;
  }

  delete [] values;
}

/* ----------------------------------------------------------------------
   unpack n lines from atom-style specific section of data file
   check that atom IDs are > 0 and <= map_tag_max
   call style-specific routine to parse line
------------------------------------------------------------------------- */

void Atom::data_bonus(int n, char *buf, AtomVec *avec_bonus)
{
  int j,m,tagdata;
  char *next;

  next = strchr(buf,'\n');
  *next = '\0';
  int nwords = count_words(buf);
  *next = '\n';

  if (nwords != avec_bonus->size_data_bonus)
    error->all(FLERR,"Incorrect bonus data format in data file");

  char **values = new char*[nwords];

  // loop over lines of bonus atom data
  // tokenize the line into values
  // if I own atom tag, unpack its values

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');

    values[0] = strtok(buf," \t\n\r\f");
    for (j = 1; j < nwords; j++)
      values[j] = strtok(NULL," \t\n\r\f");

    tagdata = atoi(values[0]);
    if (tagdata <= 0 || tagdata > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Bonus section of data file");

    // ok to call child's data_atom_bonus() method thru parent avec_bonus,
    // since data_bonus() was called with child ptr, and method is virtual

    if ((m = map(tagdata)) >= 0) avec_bonus->data_atom_bonus(m,&values[1]);

    buf = next + 1;
  }

  delete [] values;
}

/* ----------------------------------------------------------------------
   check that atom IDs are > 0 and <= map_tag_max
------------------------------------------------------------------------- */

void Atom::data_bonds(int n, char *buf)
{
  int m,tmp,itype,atom1,atom2;
  char *next;
  int newton_bond = force->newton_bond;

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');
    *next = '\0';
    sscanf(buf,"%d %d %d %d",&tmp,&itype,&atom1,&atom2);
    if (atom1 <= 0 || atom1 > map_tag_max ||
        atom2 <= 0 || atom2 > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Bonds section of data file");
    if (itype <= 0 || itype > nbondtypes)
      error->one(FLERR,"Invalid bond type in Bonds section of data file");
    if ((m = map(atom1)) >= 0) {
      bond_type[m][num_bond[m]] = itype;
      bond_atom[m][num_bond[m]] = atom2;
      num_bond[m]++;
    }
    if (newton_bond == 0) {
      if ((m = map(atom2)) >= 0) {
        bond_type[m][num_bond[m]] = itype;
        bond_atom[m][num_bond[m]] = atom1;
        num_bond[m]++;
      }
    }
    buf = next + 1;
  }
}

/* ----------------------------------------------------------------------
   check that atom IDs are > 0 and <= map_tag_max
------------------------------------------------------------------------- */

void Atom::data_angles(int n, char *buf)
{
  int m,tmp,itype,atom1,atom2,atom3;
  char *next;
  int newton_bond = force->newton_bond;

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');
    *next = '\0';
    sscanf(buf,"%d %d %d %d %d",&tmp,&itype,&atom1,&atom2,&atom3);
    if (atom1 <= 0 || atom1 > map_tag_max ||
        atom2 <= 0 || atom2 > map_tag_max ||
        atom3 <= 0 || atom3 > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Angles section of data file");
    if (itype <= 0 || itype > nangletypes)
      error->one(FLERR,"Invalid angle type in Angles section of data file");
    if ((m = map(atom2)) >= 0) {
      angle_type[m][num_angle[m]] = itype;
      angle_atom1[m][num_angle[m]] = atom1;
      angle_atom2[m][num_angle[m]] = atom2;
      angle_atom3[m][num_angle[m]] = atom3;
      num_angle[m]++;
    }
    if (newton_bond == 0) {
      if ((m = map(atom1)) >= 0) {
        angle_type[m][num_angle[m]] = itype;
        angle_atom1[m][num_angle[m]] = atom1;
        angle_atom2[m][num_angle[m]] = atom2;
        angle_atom3[m][num_angle[m]] = atom3;
        num_angle[m]++;
      }
      if ((m = map(atom3)) >= 0) {
        angle_type[m][num_angle[m]] = itype;
        angle_atom1[m][num_angle[m]] = atom1;
        angle_atom2[m][num_angle[m]] = atom2;
        angle_atom3[m][num_angle[m]] = atom3;
        num_angle[m]++;
      }
    }
    buf = next + 1;
  }
}

/* ----------------------------------------------------------------------
   check that atom IDs are > 0 and <= map_tag_max
------------------------------------------------------------------------- */

void Atom::data_dihedrals(int n, char *buf)
{
  int m,tmp,itype,atom1,atom2,atom3,atom4;
  char *next;
  int newton_bond = force->newton_bond;

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');
    *next = '\0';
    sscanf(buf,"%d %d %d %d %d %d",&tmp,&itype,&atom1,&atom2,&atom3,&atom4);
    if (atom1 <= 0 || atom1 > map_tag_max ||
        atom2 <= 0 || atom2 > map_tag_max ||
        atom3 <= 0 || atom3 > map_tag_max ||
        atom4 <= 0 || atom4 > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Dihedrals section of data file");
    if (itype <= 0 || itype > ndihedraltypes)
      error->one(FLERR,"Invalid dihedral type in Dihedrals section of data file");
    if ((m = map(atom2)) >= 0) {
      dihedral_type[m][num_dihedral[m]] = itype;
      dihedral_atom1[m][num_dihedral[m]] = atom1;
      dihedral_atom2[m][num_dihedral[m]] = atom2;
      dihedral_atom3[m][num_dihedral[m]] = atom3;
      dihedral_atom4[m][num_dihedral[m]] = atom4;
      num_dihedral[m]++;
    }
    if (newton_bond == 0) {
      if ((m = map(atom1)) >= 0) {
        dihedral_type[m][num_dihedral[m]] = itype;
        dihedral_atom1[m][num_dihedral[m]] = atom1;
        dihedral_atom2[m][num_dihedral[m]] = atom2;
        dihedral_atom3[m][num_dihedral[m]] = atom3;
        dihedral_atom4[m][num_dihedral[m]] = atom4;
        num_dihedral[m]++;
      }
      if ((m = map(atom3)) >= 0) {
        dihedral_type[m][num_dihedral[m]] = itype;
        dihedral_atom1[m][num_dihedral[m]] = atom1;
        dihedral_atom2[m][num_dihedral[m]] = atom2;
        dihedral_atom3[m][num_dihedral[m]] = atom3;
        dihedral_atom4[m][num_dihedral[m]] = atom4;
        num_dihedral[m]++;
      }
      if ((m = map(atom4)) >= 0) {
        dihedral_type[m][num_dihedral[m]] = itype;
        dihedral_atom1[m][num_dihedral[m]] = atom1;
        dihedral_atom2[m][num_dihedral[m]] = atom2;
        dihedral_atom3[m][num_dihedral[m]] = atom3;
        dihedral_atom4[m][num_dihedral[m]] = atom4;
        num_dihedral[m]++;
      }
    }
    buf = next + 1;
  }
}

/* ----------------------------------------------------------------------
   check that atom IDs are > 0 and <= map_tag_max
------------------------------------------------------------------------- */

void Atom::data_impropers(int n, char *buf)
{
  int m,tmp,itype,atom1,atom2,atom3,atom4;
  char *next;
  int newton_bond = force->newton_bond;

  for (int i = 0; i < n; i++) {
    next = strchr(buf,'\n');
    *next = '\0';
    sscanf(buf,"%d %d %d %d %d %d",&tmp,&itype,&atom1,&atom2,&atom3,&atom4);
    if (atom1 <= 0 || atom1 > map_tag_max ||
        atom2 <= 0 || atom2 > map_tag_max ||
        atom3 <= 0 || atom3 > map_tag_max ||
        atom4 <= 0 || atom4 > map_tag_max)
      error->one(FLERR,"Invalid atom ID in Impropers section of data file");
    if (itype <= 0 || itype > nimpropertypes)
      error->one(FLERR,"Invalid improper type in Impropers section of data file");
    if ((m = map(atom2)) >= 0) {
      improper_type[m][num_improper[m]] = itype;
      improper_atom1[m][num_improper[m]] = atom1;
      improper_atom2[m][num_improper[m]] = atom2;
      improper_atom3[m][num_improper[m]] = atom3;
      improper_atom4[m][num_improper[m]] = atom4;
      num_improper[m]++;
    }
    if (newton_bond == 0) {
      if ((m = map(atom1)) >= 0) {
        improper_type[m][num_improper[m]] = itype;
        improper_atom1[m][num_improper[m]] = atom1;
        improper_atom2[m][num_improper[m]] = atom2;
        improper_atom3[m][num_improper[m]] = atom3;
        improper_atom4[m][num_improper[m]] = atom4;
        num_improper[m]++;
      }
      if ((m = map(atom3)) >= 0) {
        improper_type[m][num_improper[m]] = itype;
        improper_atom1[m][num_improper[m]] = atom1;
        improper_atom2[m][num_improper[m]] = atom2;
        improper_atom3[m][num_improper[m]] = atom3;
        improper_atom4[m][num_improper[m]] = atom4;
        num_improper[m]++;
      }
      if ((m = map(atom4)) >= 0) {
        improper_type[m][num_improper[m]] = itype;
        improper_atom1[m][num_improper[m]] = atom1;
        improper_atom2[m][num_improper[m]] = atom2;
        improper_atom3[m][num_improper[m]] = atom3;
        improper_atom4[m][num_improper[m]] = atom4;
        num_improper[m]++;
      }
    }
    buf = next + 1;
  }
}

/* ----------------------------------------------------------------------
   allocate arrays of length ntypes
   only done after ntypes is set
------------------------------------------------------------------------- */

void Atom::allocate_type_arrays()
{
  if (avec->mass_type) {
    mass = new double[ntypes+1];
    mass_setflag = new int[ntypes+1];
    for (int itype = 1; itype <= ntypes; itype++) mass_setflag[itype] = 0;
  }
}

/* ----------------------------------------------------------------------
   set a mass and flag it as set
   called from reading of data file
------------------------------------------------------------------------- */

void Atom::set_mass(const char *str)
{
  if (mass == NULL) error->all(FLERR,"Cannot set mass for this atom style");

  int itype;
  double mass_one;
  int n = sscanf(str,"%d %lg",&itype,&mass_one);
  if (n != 2) error->all(FLERR,"Invalid mass line in data file");

  if (itype < 1 || itype > ntypes) error->all(FLERR,"Invalid type for mass set");

  mass[itype] = mass_one;
  mass_setflag[itype] = 1;

  if (mass[itype] <= 0.0) error->all(FLERR,"Invalid mass value");
}

/* ----------------------------------------------------------------------
   set a mass and flag it as set
   called from EAM pair routine
------------------------------------------------------------------------- */

void Atom::set_mass(int itype, double value)
{
  if (mass == NULL) error->all(FLERR,"Cannot set mass for this atom style");
  if (itype < 1 || itype > ntypes) error->all(FLERR,"Invalid type for mass set");

  mass[itype] = value;
  mass_setflag[itype] = 1;

  if (mass[itype] <= 0.0) error->all(FLERR,"Invalid mass value");
}

/* ----------------------------------------------------------------------
   set one or more masses and flag them as set
   called from reading of input script
------------------------------------------------------------------------- */

void Atom::set_mass(int narg, char **arg)
{
  if (mass == NULL) error->all(FLERR,"Cannot set mass for this atom style");

  int lo,hi;
  force->bounds(arg[0],ntypes,lo,hi);
  if (lo < 1 || hi > ntypes) error->all(FLERR,"Invalid type for mass set");

  for (int itype = lo; itype <= hi; itype++) {
    mass[itype] = atof(arg[1]);
    mass_setflag[itype] = 1;

    if (mass[itype] <= 0.0) error->all(FLERR,"Invalid mass value");
  }
}

/* ----------------------------------------------------------------------
   set all masses as read in from restart file
------------------------------------------------------------------------- */

void Atom::set_mass(double *values)
{
  for (int itype = 1; itype <= ntypes; itype++) {
    mass[itype] = values[itype];
    mass_setflag[itype] = 1;
  }
}

/* ----------------------------------------------------------------------
   check that all masses have been set
------------------------------------------------------------------------- */

void Atom::check_mass()
{
  if (mass == NULL) return;
  for (int itype = 1; itype <= ntypes; itype++)
    if (mass_setflag[itype] == 0) error->all(FLERR,"All masses are not set");
}

/* ----------------------------------------------------------------------
   check that radii of all particles of itype are the same
   return 1 if true, else return 0
   also return the radius value for that type
------------------------------------------------------------------------- */

int Atom::radius_consistency(int itype, double &rad)
{
  double value = -1.0;
  int flag = 0;
  for (int i = 0; i < nlocal; i++) {
    if (type[i] != itype) continue;
    if (value < 0.0) value = radius[i];
    else if (value != radius[i]) flag = 1;
  }

  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  if (flagall) return 0;

  MPI_Allreduce(&value,&rad,1,MPI_DOUBLE,MPI_MAX,world);
  return 1;
}

/* ----------------------------------------------------------------------
   check that shape of all particles of itype are the same
   return 1 if true, else return 0
   also return the 3 shape params for itype
------------------------------------------------------------------------- */

int Atom::shape_consistency(int itype,
                            double &shapex, double &shapey, double &shapez)
{
  double zero[3] = {0.0, 0.0, 0.0};
  double one[3] = {-1.0, -1.0, -1.0};
  double *shape;

  AtomVecEllipsoid *avec_ellipsoid =
    (AtomVecEllipsoid *) style_match("ellipsoid");
  AtomVecEllipsoid::Bonus *bonus = avec_ellipsoid->bonus;

  int flag = 0;
  for (int i = 0; i < nlocal; i++) {
    if (type[i] != itype) continue;
    if (ellipsoid[i] < 0) shape = zero;
    else shape = bonus[ellipsoid[i]].shape;

    if (one[0] < 0.0) {
      one[0] = shape[0];
      one[1] = shape[1];
      one[2] = shape[2];
    } else if (one[0] != shape[0] || one[1] != shape[1] || one[2] != shape[2])
      flag = 1;
  }

  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  if (flagall) return 0;

  double oneall[3];
  MPI_Allreduce(one,oneall,3,MPI_DOUBLE,MPI_MAX,world);
  shapex = oneall[0];
  shapey = oneall[1];
  shapez = oneall[2];
  return 1;
}

/* ----------------------------------------------------------------------
   reorder owned atoms so those in firstgroup appear first
   called by comm->exchange() if atom_modify first group is set
   only owned atoms exist at this point, no ghost atoms
------------------------------------------------------------------------- */

void Atom::first_reorder()
{
  // insure there is one extra atom location at end of arrays for swaps

  if (nlocal == nmax) avec->grow(0);

  // loop over owned atoms
  // nfirst = index of first atom not in firstgroup
  // when find firstgroup atom out of place, swap it with atom nfirst

  int bitmask = group->bitmask[firstgroup];
  nfirst = 0;
  while (nfirst < nlocal && mask[nfirst] & bitmask) nfirst++;

  for (int i = 0; i < nlocal; i++) {
    if (mask[i] & bitmask && i > nfirst) {
      avec->copy(i,nlocal,0);
      avec->copy(nfirst,i,0);
      avec->copy(nlocal,nfirst,0);
      while (nfirst < nlocal && mask[nfirst] & bitmask) nfirst++;
    }
  }
}

/* ----------------------------------------------------------------------
   perform spatial sort of atoms within my sub-domain
   always called between comm->exchange() and comm->borders()
   don't have to worry about clearing/setting atom->map since done in comm
------------------------------------------------------------------------- */

void Atom::sort()
{
  int i,m,n,ix,iy,iz,ibin,empty;

  // set next timestep for sorting to take place

  nextsort = (update->ntimestep/sortfreq)*sortfreq + sortfreq;

  // download data from GPU if necessary

  if (lmp->cuda && !lmp->cuda->oncpu) lmp->cuda->downloadAll();

  // re-setup sort bins if needed

  if (domain->box_change) setup_sort_bins();
  if (nbins == 1) return;

  // reallocate per-atom vectors if needed

  if (nlocal > maxnext) {
    memory->destroy(next);
    memory->destroy(permute);
    maxnext = atom->nmax;
    memory->create(next,maxnext,"atom:next");
    memory->create(permute,maxnext,"atom:permute");
  }

  // insure there is one extra atom location at end of arrays for swaps

  if (nlocal == nmax) avec->grow(0);

  // bin atoms in reverse order so linked list will be in forward order

  for (i = 0; i < nbins; i++) binhead[i] = -1;

  for (i = nlocal-1; i >= 0; i--) {
    ix = static_cast<int> ((x[i][0]-bboxlo[0])*bininvx);
    iy = static_cast<int> ((x[i][1]-bboxlo[1])*bininvy);
    iz = static_cast<int> ((x[i][2]-bboxlo[2])*bininvz);
    ix = MAX(ix,0);
    iy = MAX(iy,0);
    iz = MAX(iz,0);
    ix = MIN(ix,nbinx-1);
    iy = MIN(iy,nbiny-1);
    iz = MIN(iz,nbinz-1);
    ibin = iz*nbiny*nbinx + iy*nbinx + ix;
    next[i] = binhead[ibin];
    binhead[ibin] = i;
  }

  // permute = desired permutation of atoms
  // permute[I] = J means Ith new atom will be Jth old atom

  n = 0;
  for (m = 0; m < nbins; m++) {
    i = binhead[m];
    while (i >= 0) {
      permute[n++] = i;
      i = next[i];
    }
  }

  // current = current permutation, just reuse next vector
  // current[I] = J means Ith current atom is Jth old atom

  int *current = next;
  for (i = 0; i < nlocal; i++) current[i] = i;

  // reorder local atom list, when done, current = permute
  // perform "in place" using copy() to extra atom location at end of list
  // inner while loop processes one cycle of the permutation
  // copy before inner-loop moves an atom to end of atom list
  // copy after inner-loop moves atom at end of list back into list
  // empty = location in atom list that is currently empty

  for (i = 0; i < nlocal; i++) {
    if (current[i] == permute[i]) continue;
    avec->copy(i,nlocal,0);
    empty = i;
    while (permute[empty] != i) {
      avec->copy(permute[empty],empty,0);
      empty = current[empty] = permute[empty];
    }
    avec->copy(nlocal,empty,0);
    current[empty] = permute[empty];
  }

  // upload data back to GPU if necessary

  if (lmp->cuda && !lmp->cuda->oncpu) lmp->cuda->uploadAll();

  // sanity check that current = permute

  //int flag = 0;
  //for (i = 0; i < nlocal; i++)
  //  if (current[i] != permute[i]) flag = 1;
  //int flagall;
  //MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  //if (flagall) error->all(FLERR,"Atom sort did not operate correctly");
}

/* ----------------------------------------------------------------------
   setup bins for spatial sorting of atoms
------------------------------------------------------------------------- */

void Atom::setup_sort_bins()
{
  // binsize:
  // user setting if explicitly set
  // 1/2 of neighbor cutoff for non-CUDA
  // CUDA_CHUNK atoms/proc for CUDA
  // check if neighbor cutoff = 0.0

  double binsize;
  if (userbinsize > 0.0) binsize = userbinsize;
  else if (!lmp->cuda) binsize = 0.5 * neighbor->cutneighmax;
  else {
    if (domain->dimension == 3) {
      double vol = (domain->boxhi[0]-domain->boxlo[0]) *
        (domain->boxhi[1]-domain->boxlo[1]) *
        (domain->boxhi[2]-domain->boxlo[2]);
      binsize = pow(1.0*CUDA_CHUNK/natoms*vol,1.0/3.0);
    } else {
      double area = (domain->boxhi[0]-domain->boxlo[0]) *
        (domain->boxhi[1]-domain->boxlo[1]);
      binsize = pow(1.0*CUDA_CHUNK/natoms*area,1.0/2.0);
    }
  }
  if (binsize == 0.0) error->all(FLERR,"Atom sorting has bin size = 0.0");

  double bininv = 1.0/binsize;

  // nbin xyz = local bins
  // bbox lo/hi = bounding box of my sub-domain

  if (domain->triclinic)
    domain->bbox(domain->sublo_lamda,domain->subhi_lamda,bboxlo,bboxhi);
  else {
    bboxlo[0] = domain->sublo[0];
    bboxlo[1] = domain->sublo[1];
    bboxlo[2] = domain->sublo[2];
    bboxhi[0] = domain->subhi[0];
    bboxhi[1] = domain->subhi[1];
    bboxhi[2] = domain->subhi[2];
  }

  nbinx = static_cast<int> ((bboxhi[0]-bboxlo[0]) * bininv);
  nbiny = static_cast<int> ((bboxhi[1]-bboxlo[1]) * bininv);
  nbinz = static_cast<int> ((bboxhi[2]-bboxlo[2]) * bininv);
  if (domain->dimension == 2) nbinz = 1;

  if (nbinx == 0) nbinx = 1;
  if (nbiny == 0) nbiny = 1;
  if (nbinz == 0) nbinz = 1;

  bininvx = nbinx / (bboxhi[0]-bboxlo[0]);
  bininvy = nbiny / (bboxhi[1]-bboxlo[1]);
  bininvz = nbinz / (bboxhi[2]-bboxlo[2]);

  if (1.0*nbinx*nbiny*nbinz > INT_MAX)
    error->one(FLERR,"Too many atom sorting bins");

  nbins = nbinx*nbiny*nbinz;

  // reallocate per-bin memory if needed

  if (nbins > maxbin) {
    memory->destroy(binhead);
    maxbin = nbins;
    memory->create(binhead,maxbin,"atom:binhead");
  }
}

/* ----------------------------------------------------------------------
   register a callback to a fix so it can manage atom-based arrays
   happens when fix is created
   flag = 0 for grow, 1 for restart
------------------------------------------------------------------------- */

void Atom::add_callback(int flag)
	{
	  int ifix;

	  // find the fix
	  // if find NULL ptr:
	  //   it's this one, since it is being replaced and has just been deleted
	  //   at this point in re-creation
	  // if don't find NULL ptr:
	  //   i is set to nfix = new one currently being added at end of list

	  for (ifix = 0; ifix < modify->nfix; ifix++)
		if (modify->fix[ifix] == NULL) break;

	  // add callback to lists, reallocating if necessary

	  if (flag == 0) {
		if (nextra_grow == nextra_grow_max) 
			{
			  nextra_grow_max += DELTA;
			  memory->grow(extra_grow,nextra_grow_max,"atom:extra_grow");
			}
			extra_grow[nextra_grow] = ifix;
			nextra_grow++;
		} else if (flag == 1) 
			{
				if (nextra_restart == nextra_restart_max) 
					{
					  nextra_restart_max += DELTA;
					  memory->grow(extra_restart,nextra_restart_max,"atom:extra_restart");
					}
			extra_restart[nextra_restart] = ifix;
			nextra_restart++;
			}
	}

/* ----------------------------------------------------------------------
   unregister a callback to a fix
   happens when fix is deleted, called by its destructor
   flag = 0 for grow, 1 for restart
------------------------------------------------------------------------- */

void Atom::delete_callback(const char *id, int flag)
{
  int ifix;
  for (ifix = 0; ifix < modify->nfix; ifix++)
    if (strcmp(id,modify->fix[ifix]->id) == 0) break;

  // compact the list of callbacks

  if (flag == 0) {
    int match;
    for (match = 0; match < nextra_grow; match++)
      if (extra_grow[match] == ifix) break;
    for (int i = match; i < nextra_grow-1; i++)
      extra_grow[i] = extra_grow[i+1];
    nextra_grow--;

  } else if (flag == 1) {
    int match;
    for (match = 0; match < nextra_grow; match++)
      if (extra_restart[match] == ifix) break;
    for (int i = ifix; i < nextra_restart-1; i++)
      extra_restart[i] = extra_restart[i+1];
    nextra_restart--;
  }
}

/* ----------------------------------------------------------------------
   decrement ptrs in callback lists to fixes beyond the deleted ifix
   happens after fix is deleted
------------------------------------------------------------------------- */

void Atom::update_callback(int ifix)
{
  for (int i = 0; i < nextra_grow; i++)
    if (extra_grow[i] > ifix) extra_grow[i]--;
  for (int i = 0; i < nextra_restart; i++)
    if (extra_restart[i] > ifix) extra_restart[i]--;
}

/* ----------------------------------------------------------------------
   return a pointer to a named internal variable
   if don't recognize name, return NULL
   customize by adding names
------------------------------------------------------------------------- */

void *Atom::extract(char *name,int &len)
{
  
  len = 0;
  if (strcmp(name,"natoms") == 0) return (void *) &natoms;
  if (strcmp(name,"nlocal") == 0) return (void *) &nlocal;
  if (strcmp(name,"mass") == 0) return (void *) mass;

  len = 1;
  if (strcmp(name,"id") == 0) return (void *) tag;
  if (strcmp(name,"type") == 0) return (void *) type;
  if (strcmp(name,"rmass") == 0) return (void *) rmass;
  if (strcmp(name,"radius") == 0) return (void *) radius;
  if (strcmp(name,"density") == 0) return (void *) density;
  if (strcmp(name,"rho") == 0) return (void *) rho;  
  if (strcmp(name,"pressure") == 0) return (void *) p;  

  len = 3;
  if (strcmp(name,"x") == 0) return (void *) x;
  if (strcmp(name,"v") == 0) return (void *) v;
  if (strcmp(name,"f") == 0) return (void *) f;
  if (strcmp(name,"omega") == 0) return (void *) omega;

  len = -1;
  return NULL;
}

/* ----------------------------------------------------------------------
   return # of bytes of allocated memory
   call to avec tallies per-atom vectors
   add in global to local mapping storage
------------------------------------------------------------------------- */

bigint Atom::memory_usage()
{
  memlength = DELTA_MEMSTR;
  memory->create(memstr,memlength,"atom:memstr");
  memstr[0] = '\0';
  bigint bytes = avec->memory_usage();
  memory->destroy(memstr);

  if (map_style == 1)
    bytes += memory->usage(map_array,map_tag_max+1);
  else if (map_style == 2) {
    bytes += map_nbucket*sizeof(int);
    bytes += map_nhash*sizeof(HashElem);
  }
  if (maxnext) {
    bytes += memory->usage(next,maxnext);
    bytes += memory->usage(permute,maxnext);
  }

  return bytes;
}

/* ----------------------------------------------------------------------
   accumulate per-atom vec names in memstr, padded by spaces
   return 1 if padded str is not already in memlist, else 0
------------------------------------------------------------------------- */

int Atom::memcheck(const char *str)
{
  int n = strlen(str) + 3;
  char *padded = new char[n];
  strcpy(padded," ");
  strcat(padded,str);
  strcat(padded," ");

  if (strstr(memstr,padded)) {
    delete [] padded;
    return 0;
  }

  if (strlen(memstr) + n >= memlength) {
    memlength += DELTA_MEMSTR;
    memory->grow(memstr,memlength,"atom:memstr");
  }

  strcat(memstr,padded);
  delete [] padded;
  return 1;
}