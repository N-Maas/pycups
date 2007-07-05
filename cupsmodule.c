/*
 * cups - Python bindings for CUPS
 * Copyright (C) 2002, 2005, 2006  Tim Waugh <twaugh@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <Python.h>
#include <cups/cups.h>
#include <cups/language.h>

#include "cupsmodule.h"

#include <locale.h>

#include "cupsconnection.h"
#include "cupsppd.h"

static PyObject *cups_password_callback = NULL;

//////////////////////
// Worker functions //
//////////////////////

static int
do_model_compare (const char *a, const char *b)
{
  const char *digits = "0123456789";
  char quick_a, quick_b;
  while ((quick_a = *a) != '\0' && (quick_b = *b) != '\0') {
    int end_a = strspn (a, digits);
    int end_b = strspn (b, digits);
    int min;
    int a_is_digit = 1;
    int cmp;

    if (quick_a != quick_b && !isdigit (quick_a) && !isdigit (quick_b)) {
      if (quick_a < quick_b) return -1;
      else return 1;
    }

    if (!end_a) {
      end_a = strcspn (a, digits);
      a_is_digit = 0;
    }

    if (!end_b) {
      if (a_is_digit)
	return -1;
      end_b = strcspn (b, digits);
    } else if (!a_is_digit)
      return 1;

    if (a_is_digit) {
      int n_a = atoi (a), n_b = atoi (b);
      if (n_a < n_b) cmp = -1;
      else if (n_a == n_b) cmp = 0;
      else cmp = 1;
    } else {
      min = end_a < end_b ? end_a : end_b;
      cmp = strncmp (a, b, min);
    }

    if (!cmp) {
      if (end_a != end_b)
	return end_a < end_b ? -1 : 1;

      a += end_a;
      b += end_b;
      continue;
    }

    return cmp;
  }

  if (quick_a == '\0' && quick_b == '\0')
    return 0;

  if (quick_a == '\0')
    return -1;

  return 1;
}

static const char *
do_password_callback (const char *prompt)
{
  static char *password;

  PyObject *args;
  PyObject *result;
  const char *pwval;

  args = Py_BuildValue ("(s)", prompt);
  result = PyEval_CallObject (cups_password_callback, args);
  Py_DECREF (args);
  if (result == NULL)
    return "";

  if (password) {
    free (password);
    password = NULL;
  }

  pwval = PyString_AsString (result);
  password = strdup (pwval);
  Py_DECREF (result);
  if (!password)
    return "";
  
  return password;
}

//////////////////////////
// Module-level methods //
//////////////////////////

static PyObject *
cups_modelSort (PyObject *self, PyObject *args)
{
  char *a, *b;

  if (!PyArg_ParseTuple (args, "ss", &a, &b))
    return NULL;

  return Py_BuildValue ("i", do_model_compare (a, b));
}

static PyObject *
cups_setUser (PyObject *self, PyObject *args)
{
  const char *user;

  if (!PyArg_ParseTuple (args, "s", &user))
    return NULL;

  cupsSetUser (user);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
cups_setServer (PyObject *self, PyObject *args)
{
  const char *server;

  if (!PyArg_ParseTuple (args, "s", &server))
    return NULL;

  cupsSetServer (server);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
cups_setPort (PyObject *self, PyObject *args)
{
  int port;

  if (!PyArg_ParseTuple (args, "i", &port))
    return NULL;

  ippSetPort (port);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
cups_setEncryption (PyObject *self, PyObject *args)
{
  int e;
  if (!PyArg_ParseTuple (args, "i", &e))
    return NULL;

  cupsSetEncryption (e);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
cups_getUser (PyObject *self)
{
  return PyString_FromString (cupsUser ());
}

static PyObject *
cups_getServer (PyObject *self)
{
  return PyString_FromString (cupsServer ());
}

static PyObject *
cups_getPort (PyObject *self)
{
  return Py_BuildValue ("i", ippPort ());
}

static PyObject *
cups_getEncryption (PyObject *self)
{
  return Py_BuildValue ("i", cupsEncryption ());
}

static PyObject *
cups_setPasswordCB (PyObject *self, PyObject *args)
{
  PyObject *cb;

  if (!PyArg_ParseTuple (args, "O:cups_setPasswordCB", &cb))
    return NULL;

  if (!PyCallable_Check (cb)) {
    PyErr_SetString (PyExc_TypeError, "Parameter must be callable");
    return NULL;
  }

  Py_XINCREF (cb);
  Py_XDECREF (cups_password_callback);
  cups_password_callback = cb;
  cupsSetPasswordCB (do_password_callback);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyMethodDef CupsMethods[] = {
  { "modelSort", cups_modelSort, METH_VARARGS,
    "Sort two model strings." },

  { "setUser", cups_setUser, METH_VARARGS,
    "Set user to connect as." },

  { "setServer", cups_setServer, METH_VARARGS,
    "Set server to connect to." },

  { "setPort", cups_setPort, METH_VARARGS,
    "Set IPP port to connect to." },

  { "setEncryption", cups_setEncryption, METH_VARARGS,
    "Set encryption policy." },

  { "getUser", (PyCFunction) cups_getUser, METH_NOARGS,
    "Get user to connect as." },

  { "getServer", (PyCFunction) cups_getServer, METH_NOARGS,
    "Get server to connect to." },

  { "getPort", (PyCFunction) cups_getPort, METH_NOARGS,
    "Get IPP port to connect to." },

  { "getEncryption", (PyCFunction) cups_getEncryption, METH_NOARGS,
    "Get encryption policy." },

  { "setPasswordCB", cups_setPasswordCB, METH_VARARGS,
    "Set user to connect as." },

  { NULL, NULL, 0, NULL }
};

void
initcups (void)
{
  PyObject *m = Py_InitModule ("cups", CupsMethods);
  PyObject *d = PyModule_GetDict (m);

  // Connection type
  cups_ConnectionType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&cups_ConnectionType) < 0)
    return;

  PyModule_AddObject (m, "Connection",
		      (PyObject *)&cups_ConnectionType);

  // PPD type
  cups_PPDType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&cups_PPDType) < 0)
    return;

  PyModule_AddObject (m, "PPD",
		      (PyObject *)&cups_PPDType);

  // Option type
  cups_OptionType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&cups_OptionType) < 0)
    return;

  PyModule_AddObject (m, "Option",
		      (PyObject *)&cups_OptionType);

  // Group type
  cups_GroupType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&cups_GroupType) < 0)
    return;

  PyModule_AddObject (m, "Group",
		      (PyObject *)&cups_GroupType);

  // Constraint type
  cups_ConstraintType.tp_new = PyType_GenericNew;
  if (PyType_Ready (&cups_ConstraintType) < 0)
    return;

  PyModule_AddObject (m, "Constraint",
		      (PyObject *)&cups_ConstraintType);

  // Constants

#define INT_CONSTANT(name)					\
  PyDict_SetItemString (d, #name, PyInt_FromLong (name))

  // CUPS printer types
  INT_CONSTANT (CUPS_PRINTER_LOCAL);
  INT_CONSTANT (CUPS_PRINTER_CLASS);
  INT_CONSTANT (CUPS_PRINTER_REMOTE);
  INT_CONSTANT (CUPS_PRINTER_BW);
  INT_CONSTANT (CUPS_PRINTER_COLOR);
  INT_CONSTANT (CUPS_PRINTER_DUPLEX);
  INT_CONSTANT (CUPS_PRINTER_STAPLE);
  INT_CONSTANT (CUPS_PRINTER_COPIES);
  INT_CONSTANT (CUPS_PRINTER_COLLATE);
  INT_CONSTANT (CUPS_PRINTER_PUNCH);
  INT_CONSTANT (CUPS_PRINTER_COVER);
  INT_CONSTANT (CUPS_PRINTER_BIND);
  INT_CONSTANT (CUPS_PRINTER_SORT);
  INT_CONSTANT (CUPS_PRINTER_SMALL);
  INT_CONSTANT (CUPS_PRINTER_MEDIUM);
  INT_CONSTANT (CUPS_PRINTER_LARGE);
  INT_CONSTANT (CUPS_PRINTER_VARIABLE);
  INT_CONSTANT (CUPS_PRINTER_IMPLICIT);
  INT_CONSTANT (CUPS_PRINTER_DEFAULT);
  INT_CONSTANT (CUPS_PRINTER_FAX);
  INT_CONSTANT (CUPS_PRINTER_REJECTING);
  INT_CONSTANT (CUPS_PRINTER_DELETE);
  INT_CONSTANT (CUPS_PRINTER_NOT_SHARED);
  INT_CONSTANT (CUPS_PRINTER_AUTHENTICATED);
  //INT_CONSTANT (CUPS_PRINTER_COMMANDS);
  INT_CONSTANT (CUPS_PRINTER_OPTIONS);

  // HTTP encryption
  INT_CONSTANT (HTTP_ENCRYPT_IF_REQUESTED);
  INT_CONSTANT (HTTP_ENCRYPT_NEVER);
  INT_CONSTANT (HTTP_ENCRYPT_REQUIRED);
  INT_CONSTANT (HTTP_ENCRYPT_ALWAYS);

  // PPD UI enum
  INT_CONSTANT (PPD_UI_BOOLEAN);
  INT_CONSTANT (PPD_UI_PICKONE);
  INT_CONSTANT (PPD_UI_PICKMANY);

  // Printer states
  INT_CONSTANT (IPP_PRINTER_IDLE);
  INT_CONSTANT (IPP_PRINTER_PROCESSING);
  INT_CONSTANT (IPP_PRINTER_STOPPED);

  // IPP errors
  INT_CONSTANT (IPP_OK);
  INT_CONSTANT (IPP_OK_SUBST);
  INT_CONSTANT (IPP_OK_CONFLICT);
  INT_CONSTANT (IPP_OK_IGNORED_SUBSCRIPTIONS);
  INT_CONSTANT (IPP_OK_IGNORED_NOTIFICATIONS);
  INT_CONSTANT (IPP_OK_TOO_MANY_EVENTS);
  INT_CONSTANT (IPP_OK_BUT_CANCEL_SUBSCRIPTION);
  INT_CONSTANT (IPP_OK_EVENTS_COMPLETE);
  INT_CONSTANT (IPP_REDIRECTION_OTHER_SITE);
  INT_CONSTANT (IPP_BAD_REQUEST);
  INT_CONSTANT (IPP_FORBIDDEN);
  INT_CONSTANT (IPP_NOT_AUTHENTICATED);
  INT_CONSTANT (IPP_NOT_AUTHORIZED);
  INT_CONSTANT (IPP_NOT_POSSIBLE);
  INT_CONSTANT (IPP_TIMEOUT);
  INT_CONSTANT (IPP_NOT_FOUND);
  INT_CONSTANT (IPP_GONE);
  INT_CONSTANT (IPP_REQUEST_ENTITY);
  INT_CONSTANT (IPP_REQUEST_VALUE);
  INT_CONSTANT (IPP_DOCUMENT_FORMAT);
  INT_CONSTANT (IPP_ATTRIBUTES);
  INT_CONSTANT (IPP_URI_SCHEME);
  INT_CONSTANT (IPP_CHARSET);
  INT_CONSTANT (IPP_CONFLICT);
  INT_CONSTANT (IPP_COMPRESSION_NOT_SUPPORTED);
  INT_CONSTANT (IPP_COMPRESSION_ERROR);
  INT_CONSTANT (IPP_DOCUMENT_FORMAT_ERROR);
  INT_CONSTANT (IPP_DOCUMENT_ACCESS_ERROR);
  INT_CONSTANT (IPP_ATTRIBUTES_NOT_SETTABLE);
  INT_CONSTANT (IPP_IGNORED_ALL_SUBSCRIPTIONS);
  INT_CONSTANT (IPP_TOO_MANY_SUBSCRIPTIONS);
  INT_CONSTANT (IPP_IGNORED_ALL_NOTIFICATIONS);
  INT_CONSTANT (IPP_PRINT_SUPPORT_FILE_NOT_FOUND);
  INT_CONSTANT (IPP_INTERNAL_ERROR);
  INT_CONSTANT (IPP_OPERATION_NOT_SUPPORTED);
  INT_CONSTANT (IPP_SERVICE_UNAVAILABLE);
  INT_CONSTANT (IPP_VERSION_NOT_SUPPORTED);
  INT_CONSTANT (IPP_DEVICE_ERROR);
  INT_CONSTANT (IPP_TEMPORARY_ERROR);
  INT_CONSTANT (IPP_NOT_ACCEPTING);
  INT_CONSTANT (IPP_PRINTER_BUSY);
  INT_CONSTANT (IPP_ERROR_JOB_CANCELLED);
  INT_CONSTANT (IPP_MULTIPLE_JOBS_NOT_SUPPORTED);
  INT_CONSTANT (IPP_PRINTER_IS_DEACTIVATED);

  // Limits
  INT_CONSTANT (IPP_MAX_NAME);

  // Exceptions
  HTTPError = PyErr_NewException ("cups.HTTPError", NULL, NULL);
  if (HTTPError == NULL)
    return;
  Py_INCREF (HTTPError);
  PyModule_AddObject (m, "HTTPError", HTTPError);

  IPPError = PyErr_NewException ("cups.IPPError", NULL, NULL);
  if (IPPError == NULL)
    return;
  Py_INCREF (IPPError);
  PyModule_AddObject (m, "IPPError", IPPError);
}
