/* ----------------------------- MNI Header -----------------------------------
@NAME       : minc_file.c
@DESCRIPTION: Code to do minc file handling.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : January 28, 1997 (Peter Neelin)
@MODIFIED   : 
 * $Log: minc_file.c,v $
 * Revision 1.1  2003-08-15 19:52:55  leili
 * Initial revision
 *
 * Revision 1.12  2002/04/29 15:24:53  rhoge
 * removed (mode_t) cast in minc_file - would not build on SGI's
 *
 * Revision 1.11  2002/04/08 17:26:34  rhoge
 * added additional sequence info to minc header
 *
 * Revision 1.10  2002/03/27 18:57:50  rhoge
 * added diffusion b value
 *
 * Revision 1.9  2002/03/22 19:19:36  rhoge
 * Numerous fixes -
 * - handle Numaris 4 Dicom patient name
 * - option to cleanup input files
 * - command option
 * - list-only option
 * - debug mode
 * - user supplied name, idstr
 * - anonymization
 *
 * Revision 1.8  2002/03/19 22:10:16  rhoge
 * removed time sorting for N4DCM mosaics - time is random for mosaics
 *
 * Revision 1.7  2002/03/19 13:13:56  rhoge
 * initial working mosaic support - I think time is scrambled though.
 *
 * Revision 1.6  2001/12/31 18:27:21  rhoge
 * modifications for dicomreader processing of Numaris 4 dicom files - at
 * this point code compiles without warning, but does not deal with
 * mosaiced files.  Also will probably not work at this time for Numaris
 * 3 .ima files.  dicomserver may also not be functional...
 *
 * Revision 1.5  2001/02/26 22:22:37  rhoge
 * added scanner serial number to minc file naming
 *
 * Revision 1.4  2001/02/26 13:38:22  rhoge
 * made `existing directory' warning conditional on logging
 *
 * Revision 1.3  2000/12/15 01:04:46  rhoge
 * make sure acquisition_id (series no) is 6 digit hhmmss string for meas loop
 *
 * Revision 1.2  2000/12/14 21:19:22  rhoge
 * added code to compute time spacing if measurement loop dynamic
 * scanning has been detected
 *
 * Revision 1.1.1.1  2000/11/30 02:13:15  rhoge
 * imported sources to CVS repository on amoeba
 *
 * Revision 6.1  1999/10/29 17:51:55  neelin
 * Fixed Log keyword
 *
 * Revision 6.0  1997/09/12 13:24:27  neelin
 * Release of minc version 0.6
 *
 * Revision 5.0  1997/08/21  13:25:26  neelin
 * Release of minc version 0.5
 *
 * Revision 4.0  1997/05/07  20:06:20  neelin
 * Release of minc version 0.4
 *
 * Revision 1.1  1997/03/04  20:56:47  neelin
 * Initial revision
 *
@COPYRIGHT  :
              Copyright 1997 Peter Neelin, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.
---------------------------------------------------------------------------- */

#include <dicomserver.h>
#include <ctype.h>
extern int Do_logging;

/* Global for minc history (sorry, but it was kludged in afterwards) */
char *minc_history = NULL;
int Anon;
char IdStr[512];
int UserIdStr;
char Name[512];
int UserName;

/* Define mri dimension names */
static char *mri_dim_names[] = {
   NULL, "echo_time", MItime, "phase_number", "chemical_shift", NULL};

/* Macros */
#define STRLEN(s) ((int) strlen(s))

/* ----------------------------- MNI Header -----------------------------------
@NAME       : create_minc_file
@INPUT      : minc_file - name of file to create. If NULL, a name is 
                 generated internally.
              clobber - if TRUE, any existing file will be overwritten.
              general_info - information for creating the file.
              file_prefix - string providing any directory or prefix 
                 for internally generated filename (if it is a directory,
                 then it must contain the last "/")
@OUTPUT     : output_file_name - returns a pointer to an internal area
                 containing the file name of the created file if minc_file
                 is NULL, or simply a pointer to minc_file. If NULL, then
                 nothing is returned.
@RETURNS    : id of image conversion variable (MI_ERROR in case of error).
@DESCRIPTION: Routine to create the minc file.
@METHOD     : 
@GLOBALS    : 
CALLS       : 
@CREATED    : November 26, 1993 (Peter Neelin)
@MODIFIED   : rhoge - modified to create directory for session
---------------------------------------------------------------------------- */
public int create_minc_file(char *minc_file, int clobber, 
                            General_Info *general_info,
                            char *file_prefix, char **output_file_name,
			    Loop_Type loop_type)
{
   static char temp_name[256];
   char patient_name[256];
   char patient_id[256];
   char scanner_model[256];
   char serial_no[256];
   char reg_time[256];
   char temp_str[256];
   int ix;
   char *filename;
   int minc_clobber;
   int mincid, icvid;
   Mri_Index imri;
   char scan_label[MRI_NDIMS][20];

   /* added by rhoge for study directory, new naming conventions: */

   char full_path[256];

   /* Prefixes for creating file name */
   static char *scan_prefix[MRI_NDIMS] = 
      {"sl", "e", "d", "p", "cs"};

   /* Turn off fatal errors */
   set_ncopts(NCOPTS_DEFAULT);

   /* Create the file name if needed */
   if (minc_file != NULL) {
      filename = minc_file;
   }
   else {
      /* Get patient name */
      /******************************************/
      /* Changed by Leili from "string_to_initials" to "string_to_filename" */
      /* based on people's request at MNI */ 
      if (UserName) {
	strcpy(patient_name,Name);
      } else {
	string_to_filename(general_info->patient.name, patient_name,
			   sizeof(patient_name));
      }

      if (STRLEN(patient_name) == 0) {
	(void) strcpy(patient_name, "no_name");
      }
      /*****************************************/
      /* Commented out by Leili,based on the request of people at MNI*/
      /* Get patient ID */
      //if (UserIdStr) {
      //	strcpy(patient_id,IdStr);
      //} else {
      //	string_to_initials(general_info->patient.identification, patient_id,
      //			   sizeof(patient_id));
      //}
      //if (STRLEN(patient_id) == 0) {
      //	(void) strcpy(patient_id, "no_id");
      //}
      /********************************************/
      /* Commented out by Leili,based on the request of people at MNI*/
      /* Get Scanner model */
      //string_to_filename(general_info->study.model, 
      //			 scanner_model,
      //                   sizeof(scanner_model));
      //if (STRLEN(scanner_model) == 0) {
      //   (void) strcpy(scanner_model, "no_scanner");
      //}

      /* Get Scanner serial number */
      //string_to_filename(general_info->study.serial_no, 
      //			 serial_no,
      //                   sizeof(serial_no));
      //if (STRLEN(serial_no) == 0) {
      //   (void) strcpy(serial_no, "no_serial_no");
      //}

      /* get rid of magnetom prefix, if present */

      //if (!strncmp(scanner_model,"magnetom_",9)) {
      //	for (ix=0; ix<strlen(scanner_model)+1; ix++) {
      //	  scanner_model[ix]=scanner_model[ix+9];
      //	}
      //}
      /******************************************/
      /* Get Study Time */
      string_to_filename(general_info->patient.reg_time, temp_str,
			 sizeof(temp_str));
      /* truncate to first 6 chars (hhmmss) */
      strncpy(reg_time, temp_str, 6);
      if (STRLEN(reg_time) == 0) {
         (void) strcpy(reg_time, "no_time");
      }
      reg_time[6]='\0'; /* terminate with null (strncpy does not) */

      /* Get strings for echo number, etc. */
      for (imri=0; imri < MRI_NDIMS; imri++) {
         if ((general_info->size[imri] < general_info->total_size[imri]) &&
             (general_info->size[imri] == 1)) {
            (void) sprintf(scan_label[imri], "%s%d", scan_prefix[imri],
                           general_info->default_index[imri]);
         }
         else {
            (void) strcpy(scan_label[imri], "");
         }
      }

      /* rhoge:  add session directory to prefix */
      
      (void) strcpy(full_path,file_prefix);

      /* Changed by leili, omitted the scanner info, changed - to _ , and get rid off the patients ID */
      (void) sprintf(temp_name, "%s_%s_%s/",
		     patient_name,
		     //patient_id,
		     //scanner_model,
		     //serial_no,
		     general_info->patient.reg_date,
		     reg_time);
      strcat(full_path,temp_name);


      /* if measurement loop, make sure that acquisition_id is
	 a 6 digit (hhmmss) string with leading zero if needed */

      if (loop_type == MEAS) {

	(void) sprintf(general_info->study.acquisition_id, "%06d",
		       general_info->acq_id);

      }

      /* Create file name */
      /* changed by leili, omitted the scanner info, changed - to _ */
      (void) sprintf(temp_name, "%s%s_%s_%s_%s%s%s%s%s%s_mri.mnc", 
                     full_path,
                     patient_name,
                     //patient_id,
                     //scanner_model,
		     //serial_no,
                     general_info->patient.reg_date,
                     reg_time,
                     general_info->study.acquisition_id,
                     scan_label[SLICE],
                     scan_label[ECHO],
                     scan_label[TIME],
                     scan_label[PHASE],
                     scan_label[CHEM_SHIFT]);
      filename = temp_name;

      if (Do_logging > HIGH_LOGGING) { /* rhoge */
	fprintf(stderr,"\nminc file name:  %s\n",filename);
	fprintf(stderr,"\nfile prefix:     %s\n",full_path);
	fprintf(stderr,"\npatient_name:    %s\n",patient_name);
	fprintf(stderr,"\nstudy_id:        %s\n",
		general_info->study.study_id);
	fprintf(stderr,"\nacquisition_id:  %s\n",
		general_info->study.acquisition_id);

	fprintf(stderr,"\nRegistration date:  %s\n",
		general_info->patient.reg_date);
	fprintf(stderr,"\nRegistration time:  %s\n",
		general_info->patient.reg_time);
      }
   }

   /* create the session directory if none exists */

   if (mkdir(full_path, 0777) &&
       (Do_logging > HIGH_LOGGING)) {
     (void) fprintf(stderr, "Directory %s exists...\n",full_path);
   }

   /* Set output file name */
   if (output_file_name != NULL)
      *output_file_name = filename;

   /* Set the clobber value */
   if (clobber) minc_clobber = NC_CLOBBER;
   else minc_clobber = NC_NOCLOBBER;

   /* Create the file */
   mincid = micreate(filename, minc_clobber);
   if (mincid == MI_ERROR) return MI_ERROR;

   /* Set up variables */
   setup_minc_variables(mincid, general_info,loop_type);

   /* Put the file in data mode */
   (void) ncsetfill(mincid, NC_NOFILL);
   if (ncendef(mincid) == MI_ERROR) {
      return MI_ERROR;
   }

   /* Create the icv */
   icvid = miicv_create();

   /* Set the type and range */
   (void) miicv_setint(icvid, MI_ICV_TYPE, NC_SHORT);
   if (general_info->is_signed)
      (void) miicv_setstr(icvid, MI_ICV_SIGN, MI_SIGNED);
   else
      (void) miicv_setstr(icvid, MI_ICV_SIGN, MI_UNSIGNED);
   (void) miicv_setdbl(icvid, MI_ICV_VALID_MIN, general_info->pixel_min);
   (void) miicv_setdbl(icvid, MI_ICV_VALID_MAX, general_info->pixel_max);

   /* Attach the icv */
   (void) miicv_attach(icvid, mincid, ncvarid(mincid, MIimage));

   return icvid;

}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : setup_minc_variables
@INPUT      : mincid
              general_info
@OUTPUT     : general_info
@RETURNS    : (nothing)
@DESCRIPTION: Routine to setup minc variables.
@METHOD     : 
@GLOBALS    : 
CALLS       : 
@CREATED    : November 26, 1993 (Peter Neelin)
@MODIFIED   :
---------------------------------------------------------------------------- */
public void setup_minc_variables(int mincid, General_Info *general_info,
				 Loop_Type loop_type)
{
   Mri_Index imri;
   Volume_Index ivol;
   World_Index iworld;
   int ndims;
   int dim[MAX_VAR_DIMS];
   long dimsize;
   char *dimname;
   int varid, imgid, dicomvar;
   double valid_range[2];
   char name[MAX_NC_NAME];
   int index;
   int regular;
   double separation, diff;
   Acr_Group cur_group;
   Acr_Element cur_element;
   int length;
   char *data;
   nc_type datatype;
   int is_char;
   int ich;

   /* stuff added by rhoge */
   double sum;
   double avg;

   /* Define the spatial dimension names */
   static char *spatial_dimnames[WORLD_NDIMS] = {MIxspace, MIyspace, MIzspace};

   /* Create the dimensions from slowest to fastest */

   ndims=0;
   /* Create the non-spatial dimensions (from slowest to fastest) */
   for (imri=MRI_NDIMS-1; (int) imri > SLICE; imri--) {

     /* for the TIME dimension, check if we have acquisition-loop
	dynamic scan OR a `corrected' dynamic scan */

     if ( (imri==TIME) &&
	  ((loop_type!=NONE) || (general_info->acq.num_dyn_scans>1)) ) { 

       /* for Siemens scans using the signal averaging loop for
	  multiple time points we use the TR as the time step */

       dimsize = general_info->size[TIME];
       if (general_info->size[TIME] > 1) {
         dimname = mri_dim_names[TIME];
         dim[ndims] = ncdimdef(mincid, dimname, dimsize);

	 varid = micreate_std_variable(mincid, dimname, NC_DOUBLE, 1, 
					 &dim[ndims]);
	 (void) miattputstr(mincid, varid, MIspacing, MI_REGULAR);
	 (void) miattputstr(mincid, varid, MIunits, "s");
	 if (loop_type == MEAS) {
	   /* if Meas loop, time step is not equal to TR, and
	      frames should have time values (rhoge) */
	   sum = 0.0;
	   for (index=1; index < general_info->size[TIME]; index++) {
	     
	     sum += general_info->coordinates[TIME][1]-
	       general_info->coordinates[TIME][0];

	   }
	   /* compute mean */
	   avg = sum/general_info->size[TIME];
	   (void) miattputdbl(mincid, varid, MIstep,avg);

	   /* check for uniformity of spacing */
	   regular = TRUE;
	   for (index=1; index < general_info->size[TIME]; index++) {
	     diff = general_info->coordinates[TIME][1]-
	       general_info->coordinates[TIME][0] - avg;
	     
	     if (diff < 0.0) diff = -diff;
	     if (separation != 0.0) diff /= avg;
	     if (diff > COORDINATE_EPSILON) {
               regular = FALSE;
               break;
	     }
	   }
	   if (regular)
	     (void) miattputstr(mincid, varid, MIspacing, MI_REGULAR);
	   else 
	     (void) miattputstr(mincid, varid, MIspacing, MI_IRREGULAR);
	 } else {

	   /* assume ACQ loop and use TR for time step */
	   (void) miattputdbl(mincid, varid, MIstep,
			      general_info->acq.rep_time);
	 }
	 (void) miattputdbl(mincid, varid, MIstart,0);

         general_info->image_index[TIME] = ndims;
         ndims++;
       }

     } else { /* NORMAL CODE */

       dimsize = general_info->size[imri];
       if (general_info->size[imri] > 1) {
         dimname = mri_dim_names[imri];
         dim[ndims] = ncdimdef(mincid, dimname, dimsize);
         if (imri == TIME) {
	   varid = micreate_std_variable(mincid, dimname, NC_DOUBLE, 1, 
					 &dim[ndims]);
	   (void) miattputstr(mincid, varid, MIunits, "s");
         }
         else if (imri == ECHO) {
	   varid = ncvardef(mincid, dimname, NC_DOUBLE, 1, &dim[ndims]);
	   (void) miattputstr(mincid, varid, MIvartype, MI_DIMENSION);
	   (void) miattputstr(mincid, varid, MIspacing, MI_IRREGULAR);
	   (void) miattputstr(mincid, varid, MIunits, "s");
         }
         general_info->image_index[imri] = ndims;
         ndims++;
       }
     }
   }

   /* Next the spatial dimensions */
   for (ivol=0; ivol < VOL_NDIMS; ivol++) {
      switch (ivol) {
      case VSLICE: 
         dimsize = general_info->size[SLICE];
         iworld = general_info->slice_world;
         break;
      case VROW: 
         dimsize = general_info->nrows;
         iworld = general_info->row_world;
         break;
      case VCOLUMN: 
         dimsize = general_info->ncolumns;
         iworld = general_info->column_world;
         break;
      }
      dimname = spatial_dimnames[iworld];
      dim[ndims] = ncdimdef(mincid, dimname, dimsize);
      if (ivol == VSLICE) {
         varid = micreate_std_variable(mincid, dimname, NC_DOUBLE, 
                                       1, &dim[ndims]);
         /* Check for regular slices */
         regular = TRUE;
         separation = general_info->step[general_info->slice_world];
         for (index=1; index < general_info->size[SLICE]; index++) {
            diff = general_info->coordinates[SLICE][index] -
               general_info->coordinates[SLICE][index-1] - separation;
            if (diff < 0.0) diff = -diff;
            if (separation != 0.0) diff /= separation;
            if (diff > COORDINATE_EPSILON) {
               regular = FALSE;
               break;
            }
         }
         if (regular)
            (void) miattputstr(mincid, varid, MIspacing, MI_REGULAR);
      }
      else
         varid = micreate_std_variable(mincid, dimname, NC_LONG, 0, NULL);
      (void) miattputdbl(mincid, varid, MIstep, 
                         general_info->step[iworld]);
      (void) miattputdbl(mincid, varid, MIstart, 
                         general_info->start[iworld]);
      (void) miattputstr(mincid, varid, MIspacetype, MI_NATIVE);
      (void) ncattput(mincid, varid, MIdirection_cosines, 
                      NC_DOUBLE, WORLD_NDIMS,
                      general_info->dircos[iworld]);
      if (ivol == VSLICE) {
         general_info->image_index[SLICE] = ndims;
      }
      ndims++;
   }

   /* Set up image variable */
   imgid = micreate_std_variable(mincid, MIimage, general_info->datatype,
                                 ndims, dim);
   if (general_info->is_signed)
      (void) miattputstr(mincid, imgid, MIsigntype, MI_SIGNED);
   else
      (void) miattputstr(mincid, imgid, MIsigntype, MI_UNSIGNED);
   valid_range[0] = general_info->pixel_min;
   valid_range[1] = general_info->pixel_max;
   (void) ncattput(mincid, imgid, MIvalid_range, NC_DOUBLE, 2, valid_range);
   (void) miattputstr(mincid, imgid, MIcomplete, MI_FALSE);

   /* Create image max and min variables */
   varid = micreate_std_variable(mincid, MIimagemin, NC_DOUBLE, ndims-2, dim);
   if (STRLEN(general_info->units) > 0)
      (void) miattputstr(mincid, varid, MIunits, general_info->units);
   varid = micreate_std_variable(mincid, MIimagemax, NC_DOUBLE, ndims-2, dim);
   if (STRLEN(general_info->units) > 0)
      (void) miattputstr(mincid, varid, MIunits, general_info->units);

   /* Create the patient variable */
   varid = micreate_group_variable(mincid, MIpatient);
   if (STRLEN(general_info->patient.name) > 0)
     if (Anon) {
       (void) miattputstr(mincid, varid, MIfull_name, 
			  "anonymous");
     } else {
       (void) miattputstr(mincid, varid, MIfull_name, 
			  general_info->patient.name);
     }
   if (STRLEN(general_info->patient.identification) > 0)
      (void) miattputstr(mincid, varid, MIidentification, 
                         general_info->patient.identification);
   if (STRLEN(general_info->patient.birth_date) > 0)
      (void) miattputstr(mincid, varid, MIbirthdate, 
                         general_info->patient.birth_date);
   if (STRLEN(general_info->patient.age) > 0)
      (void) miattputstr(mincid, varid, "age", 
                         general_info->patient.age);
   if (STRLEN(general_info->patient.sex) > 0)
      (void) miattputstr(mincid, varid, MIsex, 
                         general_info->patient.sex);
   if (general_info->patient.weight != -DBL_MAX) 
      (void) miattputdbl(mincid, varid, MIweight, 
                         general_info->patient.weight);

   /* Create the study variable */
   varid = micreate_group_variable(mincid, MIstudy);

   /* rhoge: fixed date/time to reflect study */
   if (STRLEN(general_info->patient.reg_date) > 0)
      (void) miattputstr(mincid, varid, "start_date", 
                         general_info->patient.reg_date);
   if (STRLEN(general_info->patient.reg_time) > 0)
      (void) miattputstr(mincid, varid, "start_time", 
                         general_info->patient.reg_time);
   if (STRLEN(general_info->study.modality) > 0)
      (void) miattputstr(mincid, varid, MImodality, 
                         general_info->study.modality);
   if (STRLEN(general_info->study.manufacturer) > 0)
      (void) miattputstr(mincid, varid, "manufacturer", 
                         general_info->study.manufacturer);
   if (STRLEN(general_info->study.model) > 0)
      (void) miattputstr(mincid, varid, "model", 
                         general_info->study.model);
   if (general_info->study.field_value != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "field_value", 
                         general_info->study.field_value);
   if (STRLEN(general_info->study.software_version) > 0)
      (void) miattputstr(mincid, varid, "software_version", 
                         general_info->study.software_version);
   if (STRLEN(general_info->study.serial_no) > 0)
      (void) miattputstr(mincid, varid, "serial_no", 
                         general_info->study.serial_no);
   if (STRLEN(general_info->study.calibration_date) > 0)
      (void) miattputstr(mincid, varid, "calibration_date", 
                         general_info->study.calibration_date);
   if (STRLEN(general_info->study.institution) > 0)
      (void) miattputstr(mincid, varid, MIinstitution, 
                         general_info->study.institution);
   if (STRLEN(general_info->study.station_id) > 0)
      (void) miattputstr(mincid, varid, MIstation_id, 
                         general_info->study.station_id);
   if (STRLEN(general_info->study.referring_physician) > 0)
      (void) miattputstr(mincid, varid, MIreferring_physician, 
                         general_info->study.referring_physician);

   if (STRLEN(general_info->study.performing_physician) > 0)
      (void) miattputstr(mincid, varid, "performing_physician", 
                         general_info->study.referring_physician);
   if (STRLEN(general_info->study.operator) > 0)
      (void) miattputstr(mincid, varid, "operator", 
                         general_info->study.operator);

   if (STRLEN(general_info->study.procedure) > 0)
      (void) miattputstr(mincid, varid, MIprocedure, 
                         general_info->study.procedure);
   if (STRLEN(general_info->study.study_id) > 0)
      (void) miattputstr(mincid, varid, MIstudy_id, 
                         general_info->study.study_id);

   /* Create acquisition variable */
   varid = micreate_group_variable(mincid, MIacquisition);
   if (STRLEN(general_info->study.acquisition_id) > 0)
      (void) miattputstr(mincid, varid, "acquisition_id", 
                         general_info->study.acquisition_id);
   if (STRLEN(general_info->study.start_time) > 0)
      (void) miattputstr(mincid, varid, MIstart_time, 
                         general_info->study.start_time);

   if (STRLEN(general_info->acq.scan_seq) > 0)
      (void) miattputstr(mincid, varid, MIscanning_sequence, 
                         general_info->acq.scan_seq);

   if (STRLEN(general_info->acq.seq_owner) > 0)
      (void) miattputstr(mincid, varid, "seq_owner", 
                         general_info->acq.seq_owner);

   if (STRLEN(general_info->acq.seq_descr) > 0)
      (void) miattputstr(mincid, varid, "seq_description", 
                         general_info->acq.seq_descr);


   if (STRLEN(general_info->acq.protocol_name) > 0)
      (void) miattputstr(mincid, varid, "protocol_name", 
                         general_info->acq.protocol_name);
   if (STRLEN(general_info->acq.receive_coil) > 0)
      (void) miattputstr(mincid, varid, "receive_coil", 
                         general_info->acq.receive_coil);
   if (STRLEN(general_info->acq.transmit_coil) > 0)
      (void) miattputstr(mincid, varid, "transmit_coil", 
                         general_info->acq.transmit_coil);

   if (general_info->acq.rep_time != -DBL_MAX)
      (void) miattputdbl(mincid, varid, MIrepetition_time, 
                         general_info->acq.rep_time);
   if ((general_info->acq.echo_time != -DBL_MAX) &&
       (general_info->size[ECHO] <= 1))
      (void) miattputdbl(mincid, varid, MIecho_time, 
                         general_info->acq.echo_time);
   if (general_info->acq.echo_number != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "echo_number", 
                         general_info->acq.echo_number);
   if (general_info->acq.inv_time != -DBL_MAX)
      (void) miattputdbl(mincid, varid, MIinversion_time, 
                         general_info->acq.inv_time);
   if (general_info->acq.flip_angle != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "flip_angle", 
                         general_info->acq.flip_angle);
   if (general_info->acq.slice_thickness != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "slice_thickness", 
                         general_info->acq.slice_thickness);
   if (general_info->acq.num_slices != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "num_slices", 
                         general_info->acq.num_slices);
   if (general_info->acq.b_value != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "b_value", 
                         general_info->acq.b_value);

   /* add number of dynamic scans (rhoge) */
   /* this will be relevant if we are receiving siemens scans that
      have been `cleaned up' (and hence have the correct number of
      dynamic scans inserted) */

   if (general_info->acq.num_dyn_scans != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "num_dyn_scans", 
                         general_info->acq.num_dyn_scans);

   if (general_info->acq.num_avg != -DBL_MAX)
      (void) miattputdbl(mincid, varid, MInum_averages, 
                         general_info->acq.num_avg);

   if (general_info->acq.scan_dur != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "scan_duration", 
                         general_info->acq.scan_dur);

   if (general_info->acq.ky_lines != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "ky_lines", 
                         general_info->acq.ky_lines);

   if (general_info->acq.kymax_ix != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "kymax_ix", 
                         general_info->acq.kymax_ix);

   if (general_info->acq.kymin_ix != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "kymin_ix", 
                         general_info->acq.kymin_ix);

   if (general_info->acq.kz_lines != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "kz_lines", 
                         general_info->acq.kz_lines);

   if (general_info->acq.dummy_scans != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "dummy_excitations", 
                         general_info->acq.dummy_scans);
   if (general_info->acq.imaging_freq != -DBL_MAX)
      (void) miattputdbl(mincid, varid, MIimaging_frequency, 
                         general_info->acq.imaging_freq);
   if (STRLEN(general_info->acq.imaged_nucl) > 0)
      (void) miattputstr(mincid, varid, MIimaged_nucleus, 
                         general_info->acq.imaged_nucl);

   if (general_info->acq.adc_voltage != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "adc_voltage", 
                         general_info->acq.adc_voltage);

   if (general_info->acq.adc_offset != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "adc_offset", 
                         general_info->acq.adc_offset);

   if (general_info->acq.transmit_ampl != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "transmit_ampl", 
                         general_info->acq.transmit_ampl);

   if (general_info->acq.rec_amp_gain != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "rec_amp_gain", 
                         general_info->acq.rec_amp_gain);

   if (general_info->acq.rec_preamp_gain != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "rec_preamp_gain", 
                         general_info->acq.rec_preamp_gain);

   if (general_info->acq.win_center != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "window_center", 
                         general_info->acq.win_center);

   if (general_info->acq.win_width != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "window_width", 
                         general_info->acq.win_width);

   if (general_info->acq.gy_ampl != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "gy_ampl", 
                         general_info->acq.gy_ampl);

   if (general_info->acq.gx_ampl != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "gx_ampl", 
                         general_info->acq.gx_ampl);

   if (general_info->acq.gz_ampl != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "gz_ampl", 
                         general_info->acq.gz_ampl);

   if (general_info->acq.num_phase_enc_steps != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "num_phase_enc_steps", 
                         general_info->acq.num_phase_enc_steps);
   if (general_info->acq.percent_sampling != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "percent_sampling", 
                         general_info->acq.percent_sampling);
   if (general_info->acq.percent_phase_fov != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "percent_phase_fov", 
                         general_info->acq.percent_phase_fov);
   if (general_info->acq.pixel_bandwidth != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "pixel_bandwidth", 
                         general_info->acq.pixel_bandwidth);
   if (STRLEN(general_info->acq.phase_enc_dir) > 0)
      (void) miattputstr(mincid, varid, "phase_enc_dir", 
                         general_info->acq.phase_enc_dir);
   if (general_info->acq.sar != -DBL_MAX)
      (void) miattputdbl(mincid, varid, "SAR", 
                         general_info->acq.sar);
   if (STRLEN(general_info->acq.mr_acq_type) > 0)
      (void) miattputstr(mincid, varid, "mr_acq_type", 
                         general_info->acq.mr_acq_type);
   if (STRLEN(general_info->acq.image_type) > 0)
      (void) miattputstr(mincid, varid, "image_type", 
                         general_info->acq.image_type);

   if (STRLEN(general_info->acq.comments) > 0)
      (void) miattputstr(mincid, varid, MIcomments, 
                         general_info->acq.comments);

   // this is Siemens Numaris 4 specific!
   if (STRLEN(general_info->acq.MrProt) > 0)
      (void) miattputstr(mincid, varid, "MrProt_dump", 
                         general_info->acq.MrProt);

   /* Create the dicom info variable */
   varid = ncvardef(mincid, "dicominfo", NC_LONG, 0, NULL);
   (void) miattputstr(mincid, varid, MIvartype, MI_GROUP);
   (void) miattputstr(mincid, varid, MIvarid, 
                      "MNI DICOM information variable");
   (void) miadd_child(mincid, ncvarid(mincid, MIrootvariable), varid);
   if (STRLEN(general_info->image_type_string) > 0)
      (void) miattputstr(mincid, varid, "image_type", 
                         general_info->image_type_string);
   (void) miattputdbl(mincid, varid, "window_min", general_info->window_min);
   (void) miattputdbl(mincid, varid, "window_max", general_info->window_max);

   /* Put group info in header */
   cur_group = general_info->group_list;
   dicomvar = ncvardef(mincid, DICOM_ROOT_VAR, NC_LONG, 0, NULL);
   (void) miattputstr(mincid, dicomvar, MIvartype, MI_GROUP);
   (void) miattputstr(mincid, dicomvar, MIvarid, "MNI DICOM variable");
   (void) miadd_child(mincid, ncvarid(mincid, MIrootvariable), dicomvar);
   while (cur_group != NULL) {

      /* Create variable for group */
      (void) sprintf(name, "dicom_0x%04x", acr_get_group_group(cur_group));
      varid = ncvardef(mincid, name, NC_LONG, 0, NULL);
      (void) miattputstr(mincid, varid, MIvartype, MI_GROUP);
      (void) miattputstr(mincid, varid, MIvarid, "MNI DICOM variable");
      (void) miadd_child(mincid, dicomvar, varid);

      /* Loop through elements of group */
      cur_element = acr_get_group_element_list(cur_group);
      while (cur_element != NULL) {
         (void) sprintf(name, "el_0x%04x", 
                        acr_get_element_element(cur_element));
         is_char = TRUE;
         length = acr_get_element_length(cur_element);
         data = acr_get_element_data(cur_element);
         for (ich=0; ich < length; ich++) {
            if (!isprint((int) data[ich])) {
               is_char = FALSE;
               break;
            }
         }
         if (is_char)
            datatype = NC_CHAR;
         else
            datatype = NC_BYTE;
         ncattput(mincid, varid, name, datatype, length, data);
         
         cur_element = acr_get_element_next(cur_element);
      }
      cur_group = acr_get_group_next(cur_group);
   }

   /* Create the history attribute */
   if (minc_history != NULL) {
      (void) miattputstr(mincid, NC_GLOBAL, MIhistory, minc_history);
   }

   return;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : save_minc_image
@INPUT      : icvid
              general_info
              file_info
              image
@OUTPUT     : (none)
@RETURNS    : (nothing)
@DESCRIPTION: Routine to save the image in the minc file
@METHOD     : 
@GLOBALS    : 
CALLS       : 
@CREATED    : November 26, 1993 (Peter Neelin)
@MODIFIED   :
---------------------------------------------------------------------------- */
public void save_minc_image(int icvid, General_Info *general_info, 
                            File_Info *file_info, Image_Data *image)
{
   int mincid, imgid;
   long start[MAX_VAR_DIMS], count[MAX_VAR_DIMS];
   int file_index, array_index;
   int idim;
   Mri_Index imri;
   char *dimname;
   unsigned short pvalue, pmax, pmin;
   double dvalue, maximum, minimum, scale, offset;
   long ipix, imagepix;

   /* Get the minc file id */
   (void) miicv_inqint(icvid, MI_ICV_CDFID, &mincid);
   (void) miicv_inqint(icvid, MI_ICV_VARID, &imgid);

   /* Create start and count variables */
   idim = 0;
   for (imri=MRI_NDIMS-1; (int) imri >= 0; imri--) {
      if (general_info->image_index[imri] >= 0) {
         file_index = general_info->image_index[imri];
         if (general_info->size[imri] > 1) {
            array_index = search_list(file_info->index[imri], 
                                      general_info->indices[imri],
                                      general_info->size[imri],
                                      general_info->search_start[imri]);
            if (array_index < 0) array_index = 0;
            general_info->search_start[imri] = array_index;
         }
         else {
            array_index = 0;
         }
         start[file_index] = array_index;
         count[file_index] = 1;
         idim++;
      }
   }
   start[idim] = 0;
   start[idim+1] = 0;
   count[idim] = general_info->nrows;
   count[idim+1] = general_info->ncolumns;

   /* Write out slice position */
   switch (general_info->slice_world) {
   case XCOORD: dimname = MIxspace; break;
   case YCOORD: dimname = MIyspace; break;
   case ZCOORD: dimname = MIzspace; break;
   default: dimname = MIzspace;
   }
   (void) mivarput1(mincid, ncvarid(mincid, dimname), 
                    &start[general_info->image_index[SLICE]], 
                    NC_DOUBLE, NULL, &file_info->coordinate[SLICE]);

   /* Write out time of slice, if needed */
   if (general_info->size[TIME] > 1) {
      (void) mivarput1(mincid, ncvarid(mincid, mri_dim_names[TIME]), 
                       &start[general_info->image_index[TIME]], 
                       NC_DOUBLE, NULL, &file_info->coordinate[TIME]);
   }

   /* Write out echo time of slice, if needed */
   if (general_info->size[ECHO] > 1) {
      (void) mivarput1(mincid, ncvarid(mincid, mri_dim_names[ECHO]), 
                       &start[general_info->image_index[ECHO]], 
                       NC_DOUBLE, NULL, &file_info->coordinate[ECHO]);
   }

   /* Search image for max and min */
   imagepix = general_info->nrows * general_info->ncolumns;
   pmax = 0;
   pmin = USHRT_MAX;
   for (ipix=0; ipix < imagepix; ipix++) {
      pvalue = image->data[ipix];
      if (pvalue > pmax) pmax = pvalue;
      if (pvalue < pmin) pmin = pvalue;
   }

   /* Re-scale the images */
   if (pmax > pmin)
      scale = (general_info->pixel_max - general_info->pixel_min) /
         ((double) pmax - (double) pmin);
   else
      scale = 0.0;

   offset = general_info->pixel_min - scale * (double) pmin;
   for (ipix=0; ipix < imagepix; ipix++) {
     dvalue = image->data[ipix];
     image->data[ipix] = dvalue * scale + offset;
   }

   /* Calculate new intensity max and min */
   if (general_info->pixel_max > general_info->pixel_min)
      scale = (file_info->slice_max - file_info->slice_min) /
         (general_info->pixel_max - general_info->pixel_min);
   else
      scale = 0.0;

   // debugging info for slice intensity scaling
   //   printf("general_info->pixel_max = %10.2f    file_info->slice_max = %10.2f    pmax = %u\n",general_info->pixel_max,file_info->slice_max,pmax);

   offset = file_info->slice_min - scale * general_info->pixel_min;
   minimum = (double) pmin * scale + offset;
   maximum = (double) pmax * scale + offset;

   /* Write out the max and min values */
   (void) mivarput1(mincid, ncvarid(mincid, MIimagemin), start, NC_DOUBLE,
                    NULL, &minimum);
   (void) mivarput1(mincid, ncvarid(mincid, MIimagemax), start, NC_DOUBLE,
                    NULL, &maximum);

   /* Write out the image */
   (void) miicv_put(icvid, start, count, image->data);

   return;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : close_minc_file
@INPUT      : icvid - value returned by create_minc_file
@OUTPUT     : (none)
@RETURNS    : (nothing)
@DESCRIPTION: Routine to close the minc file.
@METHOD     : 
@GLOBALS    : 
CALLS       : 
@CREATED    : November 30, 1993 (Peter Neelin)
@MODIFIED   :
---------------------------------------------------------------------------- */
public void close_minc_file(int icvid)
{
   int mincid;

   /* Get the minc file id */
   (void) miicv_inqint(icvid, MI_ICV_CDFID, &mincid);

   /* Write out the complete attribute */
   (void) miattputstr(mincid, ncvarid(mincid, MIimage), MIcomplete, MI_TRUE);

   /* Close the file */
   (void) miclose(mincid);

   (void) miicv_free(icvid);

   return;
}
