/* ----------------------------- MNI Header -----------------------------------
@NAME       : netcdf_convenience.c
@DESCRIPTION: File of convenience functions for netcdf routines. There
              is nothing MINC specific about these routines, they just
              provide convenient ways of getting netcdf data. The
              routines MI_convert_type, mivarget and mivarget1 break this
              rule by making use of the MINC variable attribute MIsigntype
              to determine the sign of an integer variable.
@METHOD     : Routines included in this file :
              public :
                 miexpand_file
                 miopen
                 micreate
                 miclose
                 miattget
                 miattget1
                 miattgetstr
                 miattputint
                 miattputdbl
                 miattputstr
                 mivarget
                 mivarget1
                 mivarput
                 mivarput1
                 miset_coords
                 mitranslate_coords
                 micopy_all_atts
                 micopy_var_def
                 micopy_var_values
                 micopy_all_var_defs
                 micopy_all_var_values
              private :
                 execute_decompress_command
                 MI_vcopy_action
@CREATED    : July 27, 1992. (Peter Neelin, Montreal Neurological Institute)
@MODIFIED   : $Log: netcdf_convenience.c,v $
@MODIFIED   : Revision 4.0  1997-05-07 20:07:52  neelin
@MODIFIED   : Release of minc version 0.4
@MODIFIED   :
 * Revision 3.3  1997/04/10  19:22:18  neelin
 * Removed redefinition of NULL and added pointer casts in appropriate places.
 *
 * Revision 3.2  1995/09/29  14:34:09  neelin
 * Modified micopy_all_atts to handle MI_ERROR being passed in as a varid.
 *
 * Revision 3.1  1995/06/12  20:43:52  neelin
 * Modified miexpand_file and miopen to try adding compression exetensions
 * to filenames if the first open fails.
 *
 * Revision 3.0  1995/05/15  19:33:12  neelin
 * Release of minc version 0.3
 *
 * Revision 2.6  1995/03/14  14:36:35  neelin
 * Got rid of broken pipe messages from miexpand_file by using exec
 * in system call.
 *
 * Revision 2.5  1995/02/08  19:14:44  neelin
 * More changes for irix 5 lint.
 *
 * Revision 2.4  1995/02/08  19:01:06  neelin
 * Moved private function declarations from minc_routines.h to appropriate file.
 *
 * Revision 2.3  1995/01/24  08:34:11  neelin
 * Added optional tempfile argument to miexpand_file.
 *
 * Revision 2.2  95/01/23  08:28:19  neelin
 * Changed name of midecompress_file to miexpand_file.
 * 
 * Revision 2.1  95/01/20  15:20:33  neelin
 * Added midecompress_file with ability to decompress only the header of a file.
 * 
 * Revision 2.0  94/09/28  10:38:13  neelin
 * Release of minc version 0.2
 * 
 * Revision 1.11  94/09/28  10:37:19  neelin
 * Pre-release
 * 
 * Revision 1.10  93/11/03  12:28:04  neelin
 * Added miopen, micreate, miclose routines.
 * miopen will uncompress files before opening them, if needed.
 * 
 * Revision 1.9  93/08/11  12:06:28  neelin
 * Added RCS logging in source.
 * 
@COPYRIGHT  :
              Copyright 1993 Peter Neelin, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.
---------------------------------------------------------------------------- */

#ifndef lint
static char rcsid[] = "$Header: /private-cvsroot/minc/libsrc/netcdf_convenience.c,v 4.0 1997-05-07 20:07:52 neelin Rel $ MINC (MNI)";
#endif

#include <minc_private.h>
#ifdef unix
#include <unistd.h>
#include <sys/wait.h>
#endif

/* Private functions */
private int execute_decompress_command(char *command, char *infile, 
                                       char *outfile, int header_only);
private int MI_vcopy_action(int ndims, long start[], long count[], 
                            long nvalues, void *var_buffer, void *caller_data);




/* ----------------------------- MNI Header -----------------------------------
@NAME       : execute_decompress_command
@INPUT      : command - command to execute
              infile - input file
              outfile - output file
              header_only - TRUE if only header of minc file is needed
@OUTPUT     : (none)
@RETURNS    : status of decompress command (zero = success)
@DESCRIPTION: Routine to execute a decompression command on a minc file.
              The command must take a file name argument and must send 
              to standard output.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines, external decompression programs
@CREATED    : January 20, 1995 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
private int execute_decompress_command(char *command, char *infile, 
                                       char *outfile, int header_only)
{
   int oldncopts;
   char whole_command[1024];
   int status;
   FILE *pipe, *output;
   int output_opened;
   char buffer[1024];
   int successful_ncopen;
   int ibuf;
   int nread;
   int processid;

#define BYTES_PER_OPEN (1024*64)
#define NUM_BUFFERS_PER_OPEN ((BYTES_PER_OPEN - 1) / sizeof(buffer) + 1)

#ifndef unix

   return 1;

#else      /* Unix */


   if (!header_only) {        /* Decompress the whole file */

      (void) sprintf(whole_command, "exec %s %s > %s 2> /dev/null", 
                     command, infile, outfile);
      status = system(whole_command);
   }
   else {                     /* We just need to decompress enough for
                                 the header */

      /* Set up the command and open the pipe (we defer opening the output
         file until we have read something) */
      (void) sprintf(whole_command, "exec %s %s 2> /dev/null", 
                     command, infile);
      pipe = popen(whole_command, "r");
      output_opened = FALSE;

      /* Loop until we have successfully opened the minc file (the header
         is all there) */
      successful_ncopen = FALSE;
      while (!successful_ncopen && !feof(pipe)) {

         /* Loop, copying buffers from pipe to file. If the file hasn't been
            opened, then open it */
         for (ibuf=0; (ibuf < NUM_BUFFERS_PER_OPEN) && 
              ((nread = 
                fread(buffer, sizeof(char), sizeof(buffer), pipe)) > 0); 
              ibuf++) {
            if (!output_opened) {
               output= fopen(outfile, "w");
               if (output == NULL) {
                  (void) fclose(pipe);
                  return 1;
               }
               output_opened = TRUE;
            }
            if (fwrite(buffer, sizeof(char), nread, output) != nread) {
               (void) fclose(output);
               (void) fclose(pipe);
               return 1;
            }
         }      /* End of for loop, copying from pipe to output */
         if (fflush(output)) {
            (void) fclose(output);
            (void) fclose(pipe);
            return 1;
         }

         /* Try to open minc file. There seems to be a bug in NetCDF 2.3.2 -
            when the header is not all present in the file, we get a core
            dump if ncopts does not have NC_FATAL set. There error is
            reported to be in NC_free_var (var.c:54), called from 
            NC_free_array (array.c:348). This is all called from 
            NC_new_cdf (cdf.c:84), in NC_free_cdf, just after the error
            return from xdr_cdf. The work-around is to fork, set fatal
            and check the return status of the child. */
         oldncopts = ncopts; ncopts = 0;
         processid = fork();
         if (!processid) {           /* Child */
            ncopts = NC_FATAL;
            status = ncopen(outfile, NC_NOWRITE);
            (void) ncclose(status);
            exit(0);
         }
         (void) waitpid(processid, &status, 0);
         if (status == 0) {
            successful_ncopen = TRUE;
         }
         ncopts = oldncopts;

      }       /* End of while, waiting for successful ncopen */

      (void) fclose(output);
      (void) fclose(pipe);

      status = !successful_ncopen;

   }          /* End of if !header_only else */

   /* Return the status */
   return status;

#endif         /* ifndef unix else */
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miexpand_file
@INPUT      : path  - name of file to open.
              tempfile - user supplied name for temporary file. If 
                 NULL, then the routine generates its own name.
              header_only - TRUE if only the header needs to be expanded.
@OUTPUT     : created_tempfile - TRUE if a temporary file was created, FALSE
                 if no file was created (either because the original file
                 was not compressed or because of an error).
@RETURNS    : name of uncompressed file (either original or a temporary file)
              or NULL if an error occurred during decompression. The caller 
              must free the string. If a system error occurs on file open or 
              the decompression type is unknown, then the original file name
              is returned.
@DESCRIPTION: Routine to expand a compressed minc file. If the original file 
              is not compressed then its name is returned. If the name of a 
              temporary file is returned, then *created_tempfile is set to
              TRUE. If header_only is TRUE, then only the header part of the 
              file will be expanded - the data part may or may not be present.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines, external decompression programs
@CREATED    : January 20, 1995 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public char *miexpand_file(char *path, char *tempfile, int header_only,
                           int *created_tempfile)
{
   typedef enum {GZIPPED, COMPRESSED, PACKED, ZIPPED, UNKNOWN} Compress_type;
   int status, oldncopts, first_ncerr, iext;
   char *newfile, *extension, *compfile;
   FILE *fp;
   Compress_type compress_type;
   static struct {
      char *extension;
      Compress_type type;
   } compression_code_list[] = {
      {".gz", GZIPPED},
      {".Z", COMPRESSED},
      {".z", PACKED},
      {".zip", ZIPPED}
   };
   static int complist_length = 
      sizeof(compression_code_list) / sizeof(compression_code_list[0]);
   static int max_compression_code_length = 5;

   MI_SAVE_ROUTINE_NAME("miexpand_file");

   /* We have not created a temporary file yet */
   *created_tempfile = FALSE;

   /* Try to open the file (close it again immediately) */
   oldncopts = ncopts; ncopts = 0;
   status = ncopen(path, NC_NOWRITE);
   if (status != MI_ERROR) {
      (void) ncclose(status);
   }
   ncopts = oldncopts;

   /* If there is no error then return the original file name */
   if (status != MI_ERROR) {
      newfile = strdup(path);
      MI_RETURN(newfile);
   }

   /* Save the error code */
   first_ncerr = ncerr;

   /* Check for the system error that doesn't show */
   if (first_ncerr == NC_NOERR) {
      fp = fopen(path, "r");
      if (fp == NULL) {
         (void) fclose(fp);
         first_ncerr = NC_SYSERR;
      }
   }

   /* Get the file extension */
   extension = strrchr(path, '.');
   if (extension == NULL) {
      extension = &path[strlen(path)];
   }

   /* Determine the type */
   compress_type = UNKNOWN;
   for (iext = 0; iext < complist_length; iext++) {
      if (STRINGS_EQUAL(extension, compression_code_list[iext].extension)) {
         compress_type = compression_code_list[iext].type;
         break;
      }
   }

   /* If there was a system error and it's not already a compressed file, 
      then maybe there exists a compressed version (with appropriate 
      extension). Loop through the list of extensions. */
   compfile = NULL;
   if ((first_ncerr == NC_SYSERR) && (compress_type == UNKNOWN)) {
      compfile = MALLOC(strlen(path) + max_compression_code_length + 2, char);
      for (iext=0; iext < complist_length; iext++) {
         (void) strcat(strcpy(compfile, path), 
                       compression_code_list[iext].extension);
         fp = fopen(compfile, "r");
         if (fp != NULL) {
            (void) fclose(fp);
            break;
         }
      }
      if (iext >= complist_length) {
         FREE(compfile);
         newfile = strdup(path);
         MI_RETURN(newfile);
      }
      compress_type = compression_code_list[iext].type;
      path = compfile;
   }

   /* If there was a system error or we don't know what to do 
      with the file, then return the original file name */
   else if ((first_ncerr == NC_SYSERR) || (compress_type == UNKNOWN)) {
      newfile = strdup(path);
      MI_RETURN(newfile);
   }

   /* Create a temporary file name */
   if (tempfile == NULL) {
      newfile = strdup(tmpnam(NULL));
   }
   else {
      newfile = strdup(tempfile);
   }
   *created_tempfile = TRUE;

   /* Try to use gunzip */
   if ((compress_type == GZIPPED) || 
       (compress_type == COMPRESSED) ||
       (compress_type == PACKED) ||
       (compress_type == ZIPPED)) {
      status = execute_decompress_command("gunzip -c", path, newfile, 
                                          header_only);
   }

   /* If that doesn't work, try something else */
   if (status != 0) {
      if (compress_type == COMPRESSED) {
         status = execute_decompress_command("zcat", path, newfile, 
                                             header_only);
      }
      else if (compress_type == PACKED) {
         status = execute_decompress_command("pcat", path, newfile, 
                                             header_only);
      }
   }

   /* Free the compressed file name, if necessary */
   if (compfile != NULL) {
      FREE(compfile);
      path = NULL;
   }

   /* Check for failure to uncompress the file */
   if (status != 0) {
      (void) remove(newfile);
      *created_tempfile = FALSE;
      FREE(newfile);
      MI_LOG_PKG_ERROR2(MI_ERR_UNCOMPRESS,"Cannot uncompress the file");
      MI_RETURN_ERROR((char *) NULL);
   }

   /* Return the new file name */
   MI_RETURN(newfile);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miopen
@INPUT      : path  - name of file to open
              mode  - NC_WRITE or NC_NOWRITE to indicate whether file should
                 be opened for write or read-only.
@OUTPUT     : (nothing)
@RETURNS    : file id or MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncopen, but will de-compress (temporarily)
              read-only files as needed.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines
@CREATED    : November 2, 1993 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miopen(char *path, int mode)
{
   int status, oldncopts, created_tempfile;
   char *tempfile;

   MI_SAVE_ROUTINE_NAME("miopen");

   /* Try to open the file */
   oldncopts = ncopts; ncopts = 0;
   status = ncopen(path, mode);
   ncopts = oldncopts;

   /* If there is no error then return */
   if (status != MI_ERROR) {
      MI_RETURN(status);
   }

   /* If the user wants to modify the file then re-generate the error 
      with ncopen */
   if (mode != NC_NOWRITE) {
      {MI_CHK_ERR(status = ncopen(path, mode))}
      MI_RETURN(status);
   }

   /* Try to expand the file */
   tempfile = miexpand_file(path, NULL, FALSE, &created_tempfile);

   /* Check for error */
   if (tempfile == NULL) {
      MI_RETURN_ERROR(MI_ERROR);
   }

   /* Open the temporary file and unlink it so that it will disappear when
      the file is closed */
   status = ncopen(tempfile, mode);
   if (created_tempfile) {
      (void) remove(tempfile);
   }
   {MI_CHK_ERR(status)}
   MI_RETURN(status);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micreate
@INPUT      : path  - name of file to create
              cmode - NC_CLOBBER or NC_NOCLOBBER
@OUTPUT     : (nothing)
@RETURNS    : file id or MI_ERROR (=-1) when an error occurs
@DESCRIPTION: A wrapper for routine nccreate, allowing future enhancements.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines
@CREATED    : November 2, 1993 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micreate(char *path, int cmode)
{
   int status;

   MI_SAVE_ROUTINE_NAME("micreate");

   {MI_CHK_ERR(status = nccreate(path, cmode))}
   MI_RETURN(status);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miclose
@INPUT      : cdfid - id of file to close
@OUTPUT     : (nothing)
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: A wrapper for routine ncclose, allowing future enhancements.
              read-only files as needed.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines
@CREATED    : November 2, 1993 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miclose(int cdfid)
{
   int status;

   MI_SAVE_ROUTINE_NAME("miclose");

   {MI_CHK_ERR(status = ncclose(cdfid))}
   MI_RETURN(status);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattget
@INPUT      : cdfid      - cdf file id
              varid      - variable id
              name       - name of attribute
              datatype   - type that calling routine wants (one of the valid
                 netcdf data types, excluding NC_CHAR)
              max_length - maximum length to return (number of elements)
@OUTPUT     : value      - value of attribute
              att_length - actual length of attribute (number of elements)
                 If NULL, then no value is returned.
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncattget, but the calling routine specifies
              the form in which data should be returned (datatype), as well
              as the maximum number of elements to get. The datatype can
              only be a numeric type. If the attribute in the file is of type 
              NC_CHAR, then an error is returned. The actual length of the
              vector is returned in att_length.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines and MI_convert_type
@CREATED    : July 27, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miattget(int cdfid, int varid, char *name, nc_type datatype,
                    int max_length, void *value, int *att_length)
{
   nc_type att_type;          /* Type of attribute */
   int actual_length;         /* Actual length of attribute */
   void *att_value;           /* Pointer to attribute value */
   int status;                /* Status of nc routine */

   MI_SAVE_ROUTINE_NAME("miattget");

   /* Inquire about the attribute */
   MI_CHK_ERR(ncattinq(cdfid, varid, name, &att_type, &actual_length))

   /* Save the actual length of the attribute */
   if (att_length != NULL)
      *att_length = actual_length;

   /* Check that the attribute type is numeric */
   if ((datatype==NC_CHAR) || (att_type==NC_CHAR)) {
      MI_LOG_PKG_ERROR2(MI_ERR_NONNUMERIC,"Non-numeric datatype");
      MI_RETURN_ERROR(MI_ERROR);
   }

   /* Check to see if the type requested is the same as the attribute type
      and that the length is less than or equal to max_length. If it is, just 
      get the value. */
   if ((datatype == att_type) && (actual_length <= max_length)) {
      MI_CHK_ERR(status=ncattget(cdfid, varid, name, value))
      MI_RETURN(status);
   }

   /* Otherwise, get space for the attribute */
   if ((att_value = MALLOC(actual_length * nctypelen(att_type), char))
                      == NULL) {
      MI_LOG_SYS_ERROR1("miattget");
      MI_RETURN_ERROR(MI_ERROR);
   }

   /* Get the attribute */
   if (ncattget(cdfid, varid, name, att_value)==MI_ERROR) {
      FREE(att_value);
      MI_RETURN_ERROR(MI_ERROR);
   }

   /* Get the values.
      Call MI_convert_type with :
         MI_convert_type(number_of_values,
                         intype,  insign,  invalues,
                         outtype, outsign, outvalues,
                         icvp) */
   status=MI_convert_type(MIN(max_length, actual_length), 
                          att_type, MI_PRIV_DEFSIGN, att_value,
                          datatype, MI_PRIV_DEFSIGN, value,
                          NULL);
   FREE(att_value);
   MI_CHK_ERR(status)
   MI_RETURN(status);
   
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattget1
@INPUT      : cdfid      - cdf file id
              varid      - variable id
              name       - name of attribute
              datatype   - type that calling routine wants (one of the valid
                 netcdf data types, excluding NC_CHAR)
@OUTPUT     : value      - value of attribute
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine miattget, but the its gets only one value
              of an attribute. If the attribute is longer, then an error
              occurs.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines and miattget
@CREATED    : July 27, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miattget1(int cdfid, int varid, char *name, nc_type datatype,
                    void *value)
{
   int att_length;      /* Actual length of the attribute */

   MI_SAVE_ROUTINE_NAME("miattget1");

   /* Get the attribute value and its actual length */
   MI_CHK_ERR(miattget(cdfid, varid, name, datatype, 1, value, &att_length))

   /* Check that the length is 1 */
   if (att_length != 1) {
      MI_LOG_PKG_ERROR2(MI_ERR_NONSCALAR,"Attribute is not a scalar value");
      MI_RETURN_ERROR(MI_ERROR);
   }

   MI_RETURN(MI_NOERROR);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattgetstr
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              name     - name of attribute
              maxlen   - maximum length of string to be copied
@OUTPUT     : value    - string returned
@RETURNS    : pointer to string value or NULL if an error occurred.
@DESCRIPTION: Gets a character attribute, copying up to maxlen characters
              into value and adding a terminating '\0' if necessary. A
              pointer to the string is returned to facilitate use.
              If the attribute is non-character, a NULL pointer is returned.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : July 28, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public char *miattgetstr(int cdfid, int varid, char *name, 
                         int maxlen, char *value)
{
   nc_type att_type;          /* Type of attribute */
   int att_length;            /* Length of attribute */
   char *att_value;           /* Pointer to attribute value */

   MI_SAVE_ROUTINE_NAME("miattgetstr");

   /* Inquire about the attribute */
   if (ncattinq(cdfid, varid, name, &att_type, &att_length)==MI_ERROR)
      MI_RETURN_ERROR((char *) NULL);

   /* Check that the attribute type is character */
   if (att_type!=NC_CHAR) {
      MI_LOG_PKG_ERROR2(MI_ERR_NONCHAR,"Non-character datatype");
      MI_RETURN_ERROR((char *) NULL);
   }

   /* Check to see if the attribute length is less than maxlen. 
      If it is, just get the value. */
   if (att_length <= maxlen) {
      if (ncattget(cdfid, varid, name, value) == MI_ERROR)
         MI_RETURN_ERROR((char *) NULL);
      /* Check the last character for a '\0' */
      if (value[att_length-1] != '\0') {
         if (att_length==maxlen)
            value[att_length-1] = '\0';
         else
            value[att_length]   = '\0';
      }
      MI_RETURN(value);
   }

   /* Otherwise, get space for the attribute */
   if ((att_value = MALLOC(att_length * nctypelen(att_type), char))
                      ==NULL) {
      MI_LOG_SYS_ERROR1("miattgetstr");
      MI_RETURN_ERROR((char *) NULL);
   }

   /* Get the attribute */
   if (ncattget(cdfid, varid, name, att_value)==MI_ERROR) {
      FREE(att_value);
      MI_RETURN_ERROR((char *) NULL);
   }

   /* Copy the attribute */
   (void) strncpy(value, att_value, (size_t) maxlen-1);
   value[maxlen-1] = '\0';

   /* Free the string */
   FREE(att_value);

   /* Return a pointer to the string */
   MI_RETURN(value);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattputint
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              name     - name of attribute
              value    - integer value for attribute
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs
@DESCRIPTION: Convenience routine for calling ncattput with integers (stored
              as long integers).
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : November 25, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miattputint(int cdfid, int varid, char *name, int value)
{
   long lvalue;

   MI_SAVE_ROUTINE_NAME("miattputint");

   lvalue = value;
   MI_CHK_ERR(ncattput(cdfid, varid, name, NC_LONG, 1, (void *) &lvalue))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattputdbl
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              name     - name of attribute
              value    - double value for attribute
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs
@DESCRIPTION: Convenience routine for calling ncattput with doubles.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : August 5, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miattputdbl(int cdfid, int varid, char *name, double value)
{
   MI_SAVE_ROUTINE_NAME("miattputdbl");

   MI_CHK_ERR(ncattput(cdfid, varid, name, NC_DOUBLE, 1, (void *) &value))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miattputstr
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              name     - name of attribute
              value    - string value for attribute
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs
@DESCRIPTION: Convenience routine for calling ncattput with character strings.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : July 28, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int miattputstr(int cdfid, int varid, char *name, char *value)
{
   MI_SAVE_ROUTINE_NAME("miattputstr");

   MI_CHK_ERR(ncattput(cdfid, varid, name, NC_CHAR, 
                       strlen(value) + 1, (void *) value))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : mivarget
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              start    - vector of coordinates of corner of hyperslab
              count    - vector of edge lengths of hyperslab
              datatype - type that calling routine wants (one of the valid
                 netcdf data types, excluding NC_CHAR)
              sign     - sign that calling routine wants (one of
                 EMPTY_STRING (or NULL) = use default sign
                 MI_SIGNED              = signed values
                 MI_UNSIGNED            = unsigned values
@OUTPUT     : values   - value of variable
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncvarget, but the calling routine specifies
              the form in which data should be returned (datatype), as well
              as the sign. The datatype can only be a numeric type. If the 
              variable in the file is of type NC_CHAR, then an error is 
              returned.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF and MINC routines
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int mivarget(int cdfid, int varid, long start[], long count[],
                    nc_type datatype, char *sign, void *values)
{
   MI_SAVE_ROUTINE_NAME("mivarget");

   MI_CHK_ERR(MI_varaccess(MI_PRIV_GET, cdfid, varid, start, count,
                           datatype, MI_get_sign_from_string(datatype, sign),
                           values, NULL, NULL))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : mivarget1
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              mindex   - vector of coordinates of value to get
              datatype - type that calling routine wants (one of the valid
                 netcdf data types, excluding NC_CHAR)
              sign     - sign that calling routine wants (one of
                 EMPTY_STRING (or NULL) = use default sign
                 MI_SIGNED              = signed values
                 MI_UNSIGNED            = unsigned values
@OUTPUT     : value    - value of variable
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncvarget1, but the calling routine specifies
              the form in which data should be returned (datatype), as well
              as the sign. The datatype can only be a numeric type. If the 
              variable in the file is of type NC_CHAR, then an error is 
              returned.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF and MINC routines
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int mivarget1(int cdfid, int varid, long mindex[],
                     nc_type datatype, char *sign, void *value)
{
   long count[MAX_VAR_DIMS];

   MI_SAVE_ROUTINE_NAME("mivarget1");

   MI_CHK_ERR(MI_varaccess(MI_PRIV_GET, cdfid, varid, mindex, 
                           miset_coords(MAX_VAR_DIMS, 1L, count),
                           datatype, MI_get_sign_from_string(datatype, sign),
                           value, NULL, NULL))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : mivarput
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              start    - vector of coordinates of corner of hyperslab
              count    - vector of edge lengths of hyperslab
              datatype - type that calling routine is passing (one of the valid
                 netcdf data types, excluding NC_CHAR)
              sign     - sign that calling routine is passing (one of
                 EMPTY_STRING (or NULL) = use default sign
                 MI_SIGNED              = signed values
                 MI_UNSIGNED            = unsigned values
              values   - value of variable
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncvarput, but the calling routine specifies
              the form in which data is passes (datatype), as well
              as the sign. The datatype can only be a numeric type. If the 
              variable in the file is of type NC_CHAR, then an error is 
              returned.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF and MINC routines
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int mivarput(int cdfid, int varid, long start[], long count[],
                    nc_type datatype, char *sign, void *values)
{
   MI_SAVE_ROUTINE_NAME("mivarput");

   MI_CHK_ERR(MI_varaccess(MI_PRIV_PUT, cdfid, varid, start, count,
                           datatype, MI_get_sign_from_string(datatype, sign),
                           values, NULL, NULL))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : mivarput1
@INPUT      : cdfid    - cdf file id
              varid    - variable id
              mindex   - vector of coordinates of value to put
              datatype - type that calling routine is passing (one of the valid
                 netcdf data types, excluding NC_CHAR)
              sign     - sign that calling routine is passing (one of
                 EMPTY_STRING (or NULL) = use default sign
                 MI_SIGNED              = signed values
                 MI_UNSIGNED            = unsigned values
@OUTPUT     : value    - value of variable
@RETURNS    : MI_ERROR (=-1) when an error occurs
@DESCRIPTION: Similar to routine ncvarput1, but the calling routine specifies
              the form in which data is passed (datatype), as well
              as the sign. The datatype can only be a numeric type. If the 
              variable in the file is of type NC_CHAR, then an error is 
              returned.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF and MINC routines
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int mivarput1(int cdfid, int varid, long mindex[],
                     nc_type datatype, char *sign, void *value)
{
   long count[MAX_VAR_DIMS];

   MI_SAVE_ROUTINE_NAME("mivarput1");

   MI_CHK_ERR(MI_varaccess(MI_PRIV_PUT, cdfid, varid, mindex, 
                           miset_coords(MAX_VAR_DIMS, 1L, count),
                           datatype, MI_get_sign_from_string(datatype, sign),
                           value, NULL, NULL))
   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : miset_coords
@INPUT      : nvals  - number of values in coordinate vector to set
              value  - value to which coordinates should be set
@OUTPUT     : coords - coordinate vector
@RETURNS    : pointer to coords.
@DESCRIPTION: Sets nvals entries of the vector coords to value.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public long *miset_coords(int nvals, long value, long coords[])
{
   int i;

   MI_SAVE_ROUTINE_NAME("miset_coords");

   for (i=0; i<nvals; i++) {
      coords[i] = value;
   }
   MI_RETURN(coords);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : mitranslate_coords
@INPUT      : cdfid     - cdf file id
              invar     - variable subscripted by incoords
              incoords  - coordinates to be copied
              outvar    - variable subscripted by outcoords
@OUTPUT     : outcoords - new coordinates
@RETURNS    : pointer to outcoords or NULL if an error occurred.
@DESCRIPTION: Translates the coordinate vector used for subscripting one
              variable (invar) to a coordinate vector that can be used to
              subscript another (outvar). This is useful when two variables
              have similar dimensions, but not necessarily the same order of
              dimensions. If invar has a dimension that is not in outvar, then
              the corresponding coordinate is ignored. If outvar has a
              dimension that is not in invar, then the corresponding coordinate
              is not modified.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : July 29, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public long *mitranslate_coords(int cdfid, 
                                int invar,  long incoords[],
                                int outvar, long outcoords[])
{
   int in_ndims, in_dim[MAX_VAR_DIMS], out_ndims, out_dim[MAX_VAR_DIMS];
   int i,j;

   MI_SAVE_ROUTINE_NAME("mitranslate_coords");

   /* Inquire about the dimensions of the two variables */
   if ( (ncvarinq(cdfid, invar,  NULL, NULL, &in_ndims,  in_dim,  NULL)
                    == MI_ERROR) ||
        (ncvarinq(cdfid, outvar, NULL, NULL, &out_ndims, out_dim, NULL)
                    == MI_ERROR)) {
      MI_RETURN_ERROR((long *) NULL);
   }

   /* Loop through out_dim, looking for dimensions in in_dim */
   for (i=0; i<out_ndims; i++) {
      for (j=0; j<in_ndims; j++) {
         if (out_dim[i]==in_dim[j]) break;
      }
      /* If we found the dimension, then copy it */
      if (j<in_ndims) {
         outcoords[i] = incoords[j];
      }
   }

   MI_RETURN(outcoords);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micopy_all_atts
@INPUT      : incdfid  - input cdf file id
              invarid  - input variable id
              outcdfid - output cdf file id
              outvarid - output variable id
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs
@DESCRIPTION: Copies all of the attributes of one variable to another.
              Attributes that already exist in outvarid are not copied.
              outcdfid must be in define mode. This routine will only copy
              global attributes to global attributes or variable attributes
              to variable attributes, but not variable to global or global
              to variable. This means that one can safely use
                 micopy_all_atts(inmincid, ncvarid(inmincid, varname),...)
              if outvarid is known to exist (the problem arises because
              MI_ERROR == NC_GLOBAL). No error is flagged if the a bad
              combination is given.

@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micopy_all_atts(int incdfid, int invarid, 
                           int outcdfid, int outvarid)
{
   int num_atts;             /* Number of attributes */
   char name[MAX_NC_NAME];   /* Name of attribute */
   int oldncopts;            /* Old value of ncopts */
   int status;               /* Status returned by function call */
   int i;

   MI_SAVE_ROUTINE_NAME("micopy_all_atts");

   /* Only allow copying of global attributes to global attributes or
      variable attributes to variable attributes - this makes
      micopy_all_atts(inmincid, ncvarid(inmincid, varname),...) safer, since
      MI_ERROR == NC_GLOBAL */
   if (((invarid == NC_GLOBAL) || (outvarid == NC_GLOBAL)) &&
       (invarid != outvarid)) {
      MI_RETURN(MI_NOERROR);
   }

   /* Inquire about the number of input variable attributes */
   if (invarid != NC_GLOBAL)
      {MI_CHK_ERR(ncvarinq(incdfid, invarid, 
                           NULL, NULL, NULL, NULL, &num_atts))}
   else
      {MI_CHK_ERR(ncinquire(incdfid, NULL, NULL, &num_atts, NULL))}

   /* Loop through input attributes */
   for (i=0; i<num_atts; i++){
      
      /* Get the attribute name */
      MI_CHK_ERR(ncattname(incdfid, invarid, i, name))

      /* Check to see if it is in the output file. We must set and reset
         ncopts to avoid surprises to the calling program. (This is no
         longer needed with new MI_SAVE_ROUTINE_NAME macro). */
      oldncopts=ncopts; ncopts=0;
      status=ncattinq(outcdfid, outvarid, name, NULL, NULL);
      ncopts=oldncopts;

      /* If the attribute does not exist, copy it */
      if (status == MI_ERROR) {
         MI_CHK_ERR(ncattcopy(incdfid, invarid, name, outcdfid, outvarid))
      }
   }

   MI_RETURN(MI_NOERROR);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micopy_var_def
@INPUT      : incdfid  - input cdf file id
              invarid  - input variable id
              outcdfid - output cdf file id
@OUTPUT     : (none)
@RETURNS    : Variable id of variable created in outcdfid or MI_ERROR (=-1)
              if an error occurs.
@DESCRIPTION: Copies a variable definition (including attributes) from one 
              cdf file to another. outcdfid must be in define mode.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micopy_var_def(int incdfid, int invarid, int outcdfid)
{
   char varname[MAX_NC_NAME]; /* Name of variable */
   char dimname[MAX_NC_NAME]; /* Name of dimension */
   nc_type datatype;          /* Type of variable */
   int ndims;                 /* Number of dimensions of variable */
   int indim[MAX_VAR_DIMS];   /* Dimensions of input variable */
   int outdim[MAX_VAR_DIMS];  /* Dimensions of output variable */
   long insize, outsize;      /* Size of each dimension */
   int recdim;                /* Dimension that is unlimited for input file */
   int outvarid;              /* Id of newly created variable */
   int oldncopts;             /* Store ncopts */
   int i;

   MI_SAVE_ROUTINE_NAME("micopy_var_def");

   /* Get name and dimensions of variable */
   MI_CHK_ERR(ncvarinq(incdfid, invarid, varname, &datatype, 
                       &ndims, indim, NULL))

   /* Get unlimited dimension for input file */
   MI_CHK_ERR(ncinquire(incdfid, NULL, NULL, NULL, &recdim))

   /* Create any new dimensions that need creating */
   for (i=0; i<ndims; i++) {

      /* Get the dimension info */
      MI_CHK_ERR(ncdiminq(incdfid, indim[i], dimname, &insize))

      /* If it exists in the output file, check the size. */
      oldncopts=ncopts; ncopts=0;
      outdim[i]=ncdimid(outcdfid, dimname);
      ncopts=oldncopts;
      if (outdim[i]!=MI_ERROR) {
         if ( (ncdiminq(outcdfid, outdim[i], NULL, &outsize)==MI_ERROR) ||
             ((insize!=0) && (outsize!=0) && (insize != outsize)) ) {
            if ((insize!=0) && (outsize!=0) && (insize != outsize))
               MI_LOG_PKG_ERROR2(MI_ERR_DIMSIZE, 
                  "Variable already has dimension of different size");
            MI_RETURN_ERROR(MI_ERROR);
         }
      }
      /* Otherwise create it */
      else {
         /* If the dimension is unlimited then try to create it unlimited
            in the output file */
         if (indim[i]==recdim) {
            oldncopts=ncopts; ncopts=0;
            outdim[i]=ncdimdef(outcdfid, dimname, NC_UNLIMITED);
            ncopts=oldncopts;
         }
         /* If it's not meant to be unlimited, or if we cannot create it
            unlimited, then create it with the current size */
         if ((indim[i]!=recdim) || (outdim[i]==MI_ERROR)) {
            MI_CHK_ERR(outdim[i]=ncdimdef(outcdfid, dimname, MAX(1,insize)))
         }
      }
   }

   /* Create a new variable */
   MI_CHK_ERR(outvarid=ncvardef(outcdfid, varname, datatype, ndims, outdim))

   /* Copy all the attributes */
   MI_CHK_ERR(micopy_all_atts(incdfid, invarid, outcdfid, outvarid))

   MI_RETURN(outvarid);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micopy_var_values
@INPUT      : incdfid  - input cdf file id
              invarid  - input variable id
              outcdfid - output cdf file id
              outvarid - output variable id (usually returned by 
                 micopy_var_def)
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs.
@DESCRIPTION: Copies the values of a variable from one cdf file to another.
              outcdfid must be in data mode. The two variables must have
              the same shape.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micopy_var_values(int incdfid, int invarid, 
                             int outcdfid, int outvarid)
{
   nc_type intype, outtype;   /* Data types */
   int inndims, outndims;     /* Number of dimensions */
   long insize[MAX_VAR_DIMS], outsize; /* Dimension sizes */
   long start[MAX_VAR_DIMS];  /* First coordinate of variable */
   int indim[MAX_VAR_DIMS];   /* Input dimensions */
   int outdim[MAX_VAR_DIMS];  /* Output dimensions */
   mi_vcopy_type stc;
   int i;

   MI_SAVE_ROUTINE_NAME("micopy_var_values");

   /* Get the dimensions of the two variables and check for compatibility */
   if ((ncvarinq(incdfid, invarid, NULL, &intype, &inndims, indim, NULL)
                     == MI_ERROR) ||
       (ncvarinq(outcdfid, outvarid, NULL, &outtype, &outndims, outdim, NULL)
                     == MI_ERROR) ||
       (intype != outtype) || (inndims != outndims)) {
      MI_LOG_PKG_ERROR2(MI_ERR_BADMATCH,
         "Variables do not match for value copy");
      MI_RETURN_ERROR(MI_ERROR);
   }

   /* Get the dimension sizes and check for compatibility */
   for (i=0; i<inndims; i++) {
      if ((ncdiminq(incdfid, indim[i], NULL, &insize[i]) == MI_ERROR) ||
          (ncdiminq(outcdfid, outdim[i], NULL, &outsize) == MI_ERROR) ||
          ((insize[i]!=0) && (outsize!=0) && (insize[i] != outsize))) {
         if ((insize[i]!=0) && (outsize!=0) && (insize[i] != outsize))
            MI_LOG_PKG_ERROR2(MI_ERR_DIMSIZE, 
               "Variables have dimensions of different size");
         MI_RETURN_ERROR(MI_ERROR);
      }
   }

   /* Copy the values */
   stc.incdfid =incdfid;
   stc.outcdfid=outcdfid;
   stc.invarid =invarid;
   stc.outvarid=outvarid;
   stc.value_size=nctypelen(intype);
   MI_CHK_ERR(MI_var_loop(inndims, miset_coords(MAX_VAR_DIMS, 0L, start),
                          insize, stc.value_size, NULL, 
                          MI_MAX_VAR_BUFFER_SIZE, &stc,
                          MI_vcopy_action))
   MI_RETURN(MI_NOERROR);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : MI_vcopy_action
@INPUT      : ndims       - number of dimensions
              var_start   - coordinate vector of corner of hyperslab
              var_count   - vector of edge lengths of hyperslab
              nvalues     - number of values in hyperslab
              var_buffer  - pointer to variable buffer
              caller_data - pointer to data from micopy_var_values
@OUTPUT     : (none)
@RETURNS    : MI_ERROR if an error occurs
@DESCRIPTION: Buffer action routine to be called by MI_var_loop, for
              use by micopy_var_values.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF and MINC routines
@CREATED    : August 3, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
private int MI_vcopy_action(int ndims, long start[], long count[], 
                            long nvalues, void *var_buffer, void *caller_data)
     /* ARGSUSED */
{
   mi_vcopy_type *ptr;       /* Pointer to data from micopy_var_values */

   MI_SAVE_ROUTINE_NAME("MI_vcopy_action");

   ptr=(mi_vcopy_type *) caller_data;

   /* Get values from input variable */
   MI_CHK_ERR(ncvarget(ptr->incdfid, ptr->invarid, start, count, var_buffer))

   /* Put values to output variable */
   MI_CHK_ERR(ncvarput(ptr->outcdfid, ptr->outvarid, start, count, 
                       var_buffer))
   MI_RETURN(MI_NOERROR);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micopy_all_var_defs
@INPUT      : incdfid       - input cdf file id
              outcdfid      - output cdf file id
              nexclude      - number of values in array excluded_vars
              excluded_vars - array of variable id's in incdfid that should
                 not be copied
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs.
@DESCRIPTION: Copies all variable definitions in file incdfid to file outcdfid
              (including attributes), excluding the variable id's listed in
              array excluded_vars. File outcdfid must be in define mode.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : August 3, 1992 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micopy_all_var_defs(int incdfid, int outcdfid, int nexclude,
                               int excluded_vars[])
{
   int num_vars;          /* Number of variables in input file */
   int varid;             /* Variable id counter */
   int i;

   MI_SAVE_ROUTINE_NAME("micopy_all_var_defs");

   /* Find out how many variables there are in the file and loop through
      them */
   MI_CHK_ERR(ncinquire(incdfid, NULL, &num_vars, NULL, NULL))

   /* Loop through variables, copying them */
   for (varid=0; varid<num_vars; varid++) {

      /* Check list of excluded variables */
      for (i=0; i<nexclude; i++) {
         if (varid==excluded_vars[i]) break;
      }

      /* If the variable is not excluded, copy its definition */
      if (i>=nexclude) {
         MI_CHK_ERR(micopy_var_def(incdfid, varid, outcdfid))
      }
   }

   /* Copy global attributes */
   for (i=0; i<nexclude; i++) {
      if (NC_GLOBAL==excluded_vars[i]) break;
   }
   if (i>=nexclude) {
      {MI_CHK_ERR(micopy_all_atts(incdfid, NC_GLOBAL, outcdfid, NC_GLOBAL))}

   }

   MI_RETURN(MI_NOERROR);
       
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : micopy_all_var_values
@INPUT      : incdfid       - input cdf file id
              outcdfid      - output cdf file id
              nexclude      - number of values in array excluded_vars
              excluded_vars - array of variable id's in incdfid that should
                 not be copied
@OUTPUT     : (none)
@RETURNS    : MI_ERROR (=-1) if an error occurs.
@DESCRIPTION: Copies all variable values in file incdfid to file outcdfid,
              excluding the variable id's listed in array excluded_vars. 
              File outcdfid must be in data mode. Usually called after
              micopy_all_var_defs with the same arguments. If a variable
              to be copied is not defined properly in outcdfid, then an
              error occurs.
@METHOD     : 
@GLOBALS    : 
@CALLS      : NetCDF routines.
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int micopy_all_var_values(int incdfid, int outcdfid, int nexclude,
                                 int excluded_vars[])
{
   int num_vars;           /* Number of variables in input file */
   int varid;              /* Variable id counter */
   int outvarid;           /* Output variable id */
   char name[MAX_NC_NAME]; /*Variable name */
   int i;

   MI_SAVE_ROUTINE_NAME("micopy_all_var_values");

   /* Find out how many variables there are in the file and loop through
      them */
   MI_CHK_ERR(ncinquire(incdfid, NULL, &num_vars, NULL, NULL))

   /* Loop through variables, copying them */
   for (varid=0; varid<num_vars; varid++) {

      /* Check list of excluded variables */
      for (i=0; i<nexclude; i++) {
         if (varid==excluded_vars[i]) break;
      }

      /* If the variable is not excluded, copy its values */
      if (i>=nexclude) {
         /* Get the input variable's name */
         MI_CHK_ERR(ncvarinq(incdfid, varid, name, NULL, NULL, NULL, NULL))
         /* Look for it in the output file */
         MI_CHK_ERR(outvarid=ncvarid(outcdfid, name))
         /* Copy the values */
         MI_CHK_ERR(micopy_var_values(incdfid, varid, outcdfid, outvarid))
      }
   }

   MI_RETURN(MI_NOERROR);
       
}
