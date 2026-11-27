project(MI-Brain)

# Keep this list in dependency order: every plug-in must come after the
# plug-ins it links against. The order is written as-is to MI-Brain.provisioning
# and plug-ins are loaded in that order. In the installed package, plug-ins only
# have RUNPATH $ORIGIN/.., so a plug-in can only be loaded if the plug-ins it
# depends on are already loaded. (The build tree hides this because it uses
# absolute RUNPATHs.)
set(_plugins
  org.blueberry.core.runtime
  org.blueberry.core.commands
  org.blueberry.core.expressions
  org.blueberry.ui.qt
  org.blueberry.ui.qt.log

  org.commontk.configadmin
  org.mitk.core.services
  org.mitk.gui.common
  org.mitk.gui.qt.common
  org.mitk.gui.qt.application
  org.mitk.gui.qt.datamanager
  org.mitk.gui.qt.ext
  org.mitk.gui.qt.extapplication
  org.mitk.gui.qt.imagenavigator
  org.mitk.gui.qt.moviemaker
  org.mitk.gui.qt.properties
  org.mitk.gui.qt.segmentation
  org.mitk.gui.qt.stdmultiwidgeteditor

  org.imeka.common.eager
  org.imeka.common.app
  org.imeka.common.view
  org.imeka.mibrain.eager
  org.imeka.mibrain.app
  org.imeka.mibrain.view
  org.imeka.mibrain.rtt.view

  org.imeka.property.eager
)

mitkFunctionCreateBlueBerryApplication(
  NAME MI-Brain
  DESCRIPTION "MI-Brainh"
  ID "org.imeka.mibrain"
  COPYRIGHT "Copyright (c) Imeka. All rights reserved."
  PLUGINS ${_plugins}
)

if(TARGET ${CMAKE_PROJECT_NAME}-autoload)
  add_dependencies(MI-Brain ${CMAKE_PROJECT_NAME}-autoload)
endif()
