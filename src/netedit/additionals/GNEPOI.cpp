/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEPOI.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Jun 2017
/// @version $Id$
///
// A class for visualizing and editing POIS in netedit (adapted from
// GUIPointOfInterest and NLHandler)
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <string>
#include <utility>
#include <utils/common/StringTokenizer.h>
#include <utils/foxtools/MFXImageHelper.h>
#include <utils/geom/Position.h>
#include <utils/geom/GeomConvHelper.h>
#include <utils/geom/GeoConvHelper.h>
#include <utils/common/MsgHandler.h>
#include <utils/xml/XMLSubSys.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/windows/GUISUMOAbstractView.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <utils/gui/div/GUIParameterTableWindow.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/images/GUITexturesHelper.h>
#include <utils/gui/images/GUIIconSubSys.h>
#include <utils/gui/globjects/GUIGlObjectStorage.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <netimport/NIImporter_SUMO.h>
#include <netwrite/NWWriter_SUMO.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/GNENet.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/GNEViewParent.h>

#include "GNEPOI.h"


// ===========================================================================
// method definitions
// ===========================================================================

GNEPOI::GNEPOI(GNENet* net, const std::string& id, const std::string& type, const RGBColor& color,
               const Position& pos, bool geo, double layer, double angle, const std::string& imgFile,
               bool relativePath, double width, double height, bool movementBlocked) :
    GUIPointOfInterest(id, type, color, pos, geo, "", 0, 0, layer, angle, imgFile, relativePath, width, height),
    GNEShape(net, SUMO_TAG_POI, movementBlocked, false),
    myGNELane(nullptr) {
    // set GEO Position
    myGEOPosition = pos;
    GeoConvHelper::getFinal().cartesian2geo(myGEOPosition);
}


GNEPOI::GNEPOI(GNENet* net, const std::string& id, const std::string& type, const RGBColor& color,
    double layer, double angle, const std::string& imgFile, bool relativePath, GNELane* lane, double posOverLane, double posLat,
    double width, double height, bool movementBlocked) :
    GUIPointOfInterest(id, type, color, Position(), false, lane->getID(), posOverLane, posLat, layer, angle, imgFile, relativePath, width, height),
    GNEShape(net, SUMO_TAG_POILANE, movementBlocked, false),
    myGNELane(lane) {
}


GNEPOI::~GNEPOI() {}


void GNEPOI::writeShape(OutputDevice& device) {
    if(myGNELane) {
        // obtain fixed position over lane
        double fixedPositionOverLane = myPosOverLane > myGNELane->getShape().length() ? myGNELane->getShape().length() : myPosOverLane < 0 ? 0 : myPosOverLane;
        // write POILane using POI::writeXML
        writeXML(device, false, 0, myGNELane->getID(), fixedPositionOverLane, myPosLat);
    } else {
        writeXML(device, myGeo);
    }
}


void
GNEPOI::moveGeometry(const Position& oldPos, const Position& offset) {
    if (!myBlockMovement) {
        if (myGNELane) {
            // first remove object from net grid
            myNet->removeGLObjectFromNet(this);
            // Calculate new position using old position
            Position newPosition = oldPos;
            newPosition.add(offset);
            myPosOverLane = myGNELane->getShape().nearest_offset_to_point2D(newPosition, false);
            // add object into net again
            myNet->addGLObjectIntoNet(this);
            // Update geometry
            updateGeometry();
        } else {
            // first remove object from net grid
            myNet->removeGLObjectFromNet(this);
            // restore old position, apply offset and refresh element
            set(oldPos);
            add(offset);
            // add object into net again
            myNet->addGLObjectIntoNet(this);
        }
    }
}


void
GNEPOI::commitGeometryMoving(const Position& oldPos, GNEUndoList* undoList) {
    if (!myBlockMovement) {
        if (myGNELane) {
            // restore old position before commit new position
            double originalPosOverLane = myGNELane->getShape().nearest_offset_to_point2D(oldPos, false);
            undoList->p_begin("position of " + toString(getTag()));
            undoList->p_add(new GNEChange_Attribute(this, SUMO_ATTR_POSITION, toString(myPosOverLane), true, toString(originalPosOverLane)));
            undoList->p_end();
        } else {
            undoList->p_begin("position of " + toString(getTag()));
            undoList->p_add(new GNEChange_Attribute(this, SUMO_ATTR_POSITION, toString(getPositionInView()), true, toString(oldPos)));
            undoList->p_end();
        }
    }
}


GNELane*
GNEPOI::getLane() const {
    return myGNELane;
}


void
GNEPOI::updateGeometry() {
    // first remove object from net grid
    myNet->removeGLObjectFromNet(this);
    if (myGNELane) {
        // obtain fixed position over lane
        double fixedPositionOverLane = myPosOverLane > myGNELane->getLaneShapeLength() ? myGNELane->getLaneShapeLength() : myPosOverLane < 0 ? 0 : myPosOverLane;
        // set new position regarding to lane
        set(myGNELane->getShape().positionAtOffset(fixedPositionOverLane * myGNELane->getLengthGeometryFactor(), -myPosLat));
    }
    // add object into net again
    myNet->addGLObjectIntoNet(this);
}


Position
GNEPOI::getPositionInView() const {
    return Position(x(), y());
}


GUIGlID
GNEPOI::getGlID() const {
    return GUIPointOfInterest::getGlID();
}


std::string
GNEPOI::getParentName() const {
    return myNet->getMicrosimID();
}


GUIGLObjectPopupMenu*
GNEPOI::getPopUpMenu(GUIMainWindow& app, GUISUMOAbstractView& parent) {
    GUIGLObjectPopupMenu* ret = new GUIGLObjectPopupMenu(app, parent, *this);
    if(myGNELane) {
        // build shape header
        buildShapePopupOptions(app, ret, getShapeType());
        // add option for convert to GNEPOI
        new FXMenuCommand(ret, ("Release from " + toString(SUMO_TAG_LANE)).c_str(), GUIIconSubSys::getIcon(ICON_LANE), &parent, MID_GNE_POI_TRANSFORM);
        return ret;
    } else {
        // build shape header
        buildShapePopupOptions(app, ret, getShapeType());
        // add option for convert to GNEPOI
        new FXMenuCommand(ret, ("Attach to nearest " + toString(SUMO_TAG_LANE)).c_str(), GUIIconSubSys::getIcon(ICON_LANE), &parent, MID_GNE_POI_TRANSFORM);
    }
    return ret;
}


GUIParameterTableWindow*
GNEPOI::getParameterWindow(GUIMainWindow& app, GUISUMOAbstractView& parent) {
    return GUIPointOfInterest::getParameterWindow(app, parent);
}


Boundary
GNEPOI::getCenteringBoundary() const {
    return GUIPointOfInterest::getCenteringBoundary();
}


void
GNEPOI::drawGL(const GUIVisualizationSettings& s) const {
    GUIPointOfInterest::drawGL(s);
    // draw lock icon if isn't in selecting mode
    if(!s.drawForSelecting) {
        drawLockIcon(*this, getType() + 0.1, 0.2);
    }
    // push matrix
    glPushName(getGlID());
    // draw an orange square mode if there is an image(see #4036)
    if (!getShapeImgFile().empty() && OptionsCont::getOptions().getBool("gui-testing")) {
        // Add a draw matrix for drawing logo
        glPushMatrix();
        glTranslated(x(), y(), getType() + 0.01);
        GLHelper::setColor(RGBColor::ORANGE);
        GLHelper::drawBoxLine(Position(0, 1), 0, 2, 1);
        glPopMatrix();

    }
    // check if dotted contour has to be drawn
    if(myNet->getViewNet()->getACUnderCursor() == this) {
        if (getShapeImgFile() != DEFAULT_IMG_FILE) {
            GLHelper::drawShapeDottedContour(getType(), *this, 2*myHalfImgWidth * s.poiSize.getExaggeration(s), 2*myHalfImgHeight * s.poiSize.getExaggeration(s));
        } else if (myPOIVertices.size() > 0) {
            glPushMatrix();
            glTranslated(x(), y(), getType() + 0.01);
            GLHelper::drawShapeDottedContour(getType(), myPOIVertices);
            glPopMatrix();
        }
    }
    // Pop name
    glPopName();
}


std::string
GNEPOI::getAttribute(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_ID:
            return myID;
        case SUMO_ATTR_COLOR:
            return toString(getShapeColor());
        case SUMO_ATTR_LANE:
            return myLane;
        case SUMO_ATTR_POSITION:
            if(myGNELane) {
                return toString(myPosOverLane);
            } else {
                return toString(*this);
            }
        case SUMO_ATTR_POSITION_LAT:
            return toString(myPosLat);
        case SUMO_ATTR_GEOPOSITION:
            return toString(myGEOPosition, gPrecisionGeo);
        case SUMO_ATTR_GEO:
            return toString(myGeo);
        case SUMO_ATTR_TYPE:
            return getShapeType();
        case SUMO_ATTR_LAYER:
            if(getShapeLayer() == Shape::DEFAULT_LAYER_POI) {
                return "default";
            } else {
                return toString(getShapeLayer());
            }
        case SUMO_ATTR_IMGFILE:
            return getShapeImgFile();
        case SUMO_ATTR_RELATIVEPATH:
            return toString(getShapeRelativePath());
        case SUMO_ATTR_WIDTH:
            return toString(getWidth());
        case SUMO_ATTR_HEIGHT:
            return toString(getHeight());
        case SUMO_ATTR_ANGLE:
            return toString(getShapeNaviDegree());
        case GNE_ATTR_BLOCK_MOVEMENT:
            return toString(myBlockMovement);
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        case GNE_ATTR_GENERIC:
            return getGenericParametersStr();
        default:
            throw InvalidArgument(toString(getTag()) + " attribute '" + toString(key) + "' not allowed");
    }
}


void
GNEPOI::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
        case SUMO_ATTR_COLOR:
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_POSITION:
        case SUMO_ATTR_POSITION_LAT:
        case SUMO_ATTR_GEOPOSITION:
        case SUMO_ATTR_GEO:
        case SUMO_ATTR_TYPE:
        case SUMO_ATTR_LAYER:
        case SUMO_ATTR_IMGFILE:
        case SUMO_ATTR_RELATIVEPATH:
        case SUMO_ATTR_WIDTH:
        case SUMO_ATTR_HEIGHT:
        case SUMO_ATTR_ANGLE:
        case GNE_ATTR_BLOCK_MOVEMENT:
        case GNE_ATTR_SELECTED:
        case GNE_ATTR_GENERIC:
            undoList->p_add(new GNEChange_Attribute(this, key, value));
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEPOI::isValid(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            return SUMOXMLDefinitions::isValidNetID(value) && (myNet->retrievePOI(value, false) == 0);
        case SUMO_ATTR_COLOR:
            return canParse<RGBColor>(value);
        case SUMO_ATTR_LANE:
            return (myNet->retrieveLane(value, false) != nullptr);
        case SUMO_ATTR_POSITION:
            if(myGNELane) {
                canParse<double>(value);
            } else {
                bool ok;
                return GeomConvHelper::parseShapeReporting(value, "user-supplied position", 0, ok, false).size() == 1;
            }
        case SUMO_ATTR_POSITION_LAT:
            return canParse<double>(value);
        case SUMO_ATTR_GEOPOSITION: {
            bool ok;
            return GeomConvHelper::parseShapeReporting(value, "user-supplied GEO position", 0, ok, false).size() == 1;
        }
        case SUMO_ATTR_GEO:
            return canParse<bool>(value);
        case SUMO_ATTR_TYPE:
            return true;
        case SUMO_ATTR_LAYER:
            if (value == "default") {
                return true;
            } else {
                return canParse<double>(value);
            }
        case SUMO_ATTR_IMGFILE:
            if (value == "") {
                return true;
            } else {
                // check that image can be loaded
                return GUITexturesHelper::getTextureID(value) != -1;
            }
        case SUMO_ATTR_RELATIVEPATH:
            return canParse<bool>(value);
        case SUMO_ATTR_WIDTH:
            return canParse<double>(value) && (parse<double>(value) >= 0);
        case SUMO_ATTR_HEIGHT:
            return canParse<double>(value) && (parse<double>(value) >= 0);
        case SUMO_ATTR_ANGLE:
            return canParse<double>(value);
        case GNE_ATTR_BLOCK_MOVEMENT:
            return canParse<bool>(value);
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_GENERIC:
            return isGenericParametersValid(value);
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool 
GNEPOI::addGenericParameter(const std::string &key, const std::string &value) {
    if(!knowsParameter(key)) {
        setParameter(key, value);
        return true;
    } else {
        return false;
    }
}


bool 
GNEPOI::removeGenericParameter(const std::string &key) {
    if(knowsParameter(key)) {
        unsetParameter(key);
        return true;
    } else {
        return false;
    }
}


bool 
GNEPOI::updateGenericParameter(const std::string &oldKey, const std::string &newKey) {
    if(knowsParameter(oldKey) && !knowsParameter(newKey)) {
        std::string value = getParameter(oldKey);
        unsetParameter(oldKey);
        setParameter(newKey, value);
        return true;
    } else {
        return false;
    }
}


bool 
GNEPOI::updateGenericParameterValue(const std::string &key, const std::string &newValue) {
    if(knowsParameter(key)) {
        setParameter(key, newValue);
        return true;
    } else {
        return false;
    }
}


std::string 
GNEPOI::getGenericParametersStr() const {
    std::string result;
    // Generate an string using the following structure: "key1=value1|key2=value2|...
    for (auto i : getParametersMap()) {
        result += i.first + "=" + i.second + "|";
    }
    // remove the last "|"
    if(!result.empty()) {
        result.pop_back();
    }
    return result;
}


std::vector<std::pair<std::string, std::string> > 
GNEPOI::getGenericParameters() const {
    std::vector<std::pair<std::string, std::string> >  result;
    // iterate over parameters map and fill result
    for (auto i : getParametersMap()) {
        result.push_back(std::make_pair(i.first, i.second));
    }
    return result;
}


void 
GNEPOI::setGenericParametersStr(const std::string &value) {
    // clear parameters
    clearParameter();
    // separate value in a vector of string using | as separator
    std::vector<std::string> parsedValues;
    StringTokenizer stValues(value, "|", true);
    while (stValues.hasNext()) {
        parsedValues.push_back(stValues.next());
    }
    // check that parsed values (A=B)can be parsed in generic parameters 
    for(auto i : parsedValues) {
        std::vector<std::string> parsedParameters;
        StringTokenizer stParam(i, "=", true);
        while (stParam.hasNext()) {
            parsedParameters.push_back(stParam.next());
        }
        // Check that parsed parameters are exactly two and contains valid chracters
        if(parsedParameters.size() == 2 && SUMOXMLDefinitions::isValidGenericParameterKey(parsedParameters.front()) && SUMOXMLDefinitions::isValidGenericParameterValue(parsedParameters.back())) {
            setParameter(parsedParameters.front(), parsedParameters.back());
        }
    }
}

// ===========================================================================
// private
// ===========================================================================

void
GNEPOI::setAttribute(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID: {
            std::string oldID = myID;
            myID = value;
            myNet->changeShapeID(this, oldID);
            break;
        }
        case SUMO_ATTR_COLOR:
            setShapeColor(parse<RGBColor>(value));
            break;
        case SUMO_ATTR_LANE:
            myLane = value;
            myGNELane->removeShapeChild(this);
            myGNELane = myNet->retrieveLane(value);
            myGNELane->addShapeChild(this);
            updateGeometry();
            break;
        case SUMO_ATTR_POSITION: {
            if(myGNELane) {
                myPosOverLane = parse<double>(value);
            } else {
                bool ok = true;
                set(GeomConvHelper::parseShapeReporting(value, "netedit-given", 0, ok, false)[0]);
                // set GEO Position
                myGEOPosition = *this;
                GeoConvHelper::getFinal().cartesian2geo(myGEOPosition);
            }
            break;
        }
        case SUMO_ATTR_POSITION_LAT:
            myPosLat = parse<double>(value);
            break;
        case SUMO_ATTR_GEOPOSITION: {
            bool ok = true;
            myGEOPosition = GeomConvHelper::parseShapeReporting(value, "netedit-given", 0, ok, false)[0];
            // set cartesian Position
            set(myGEOPosition);
            GeoConvHelper::getFinal().x2cartesian_const(*this);
            break;
        }
        case SUMO_ATTR_GEO:
            myGeo = parse<bool>(value);
            break;
        case SUMO_ATTR_TYPE:
            setShapeType(value);
            break;
        case SUMO_ATTR_LAYER:
            if(value == "default") {
                setShapeLayer(Shape::DEFAULT_LAYER_POI);
            } else {
                setShapeLayer(parse<double>(value));
            }
            break;
        case SUMO_ATTR_IMGFILE:
            setShapeImgFile(value);
            // all textures must be refresh
            GUITexturesHelper::clearTextures();
            break;
        case SUMO_ATTR_RELATIVEPATH:
            setShapeRelativePath(parse<bool>(value));
            break;
        case SUMO_ATTR_WIDTH:
            setWidth(parse<double>(value));
            break;
        case SUMO_ATTR_HEIGHT:
            setHeight(parse<double>(value));
            break;
        case SUMO_ATTR_ANGLE:
            setShapeNaviDegree(parse<double>(value));
            break;
        case GNE_ATTR_BLOCK_MOVEMENT:
            myBlockMovement = parse<bool>(value);
            break;
        case GNE_ATTR_SELECTED:
            if(parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_GENERIC:
            setGenericParametersStr(value);
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " attribute '" + toString(key) + "' not allowed");
    }
    // After setting attribute always update Geometry
    updateGeometry();
}


void 
GNEPOI::mouseOverObject(const GUIVisualizationSettings&) const {
}


/****************************************************************************/
