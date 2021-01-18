/**
 * @file
 * @brief Source file for the Keyframe class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TrackedObjectBBox.h"
#include "Clip.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <functional>

using namespace std;
using namespace openshot;


// Default Constructor that sets the bounding-box displacement as 0 and the scales as 1 for the first frame
TrackedObjectBBox::TrackedObjectBBox() : delta_x(0.0), delta_y(0.0), scale_x(1.0), scale_y(1.0), rotation(0.0)
{
    this->TimeScale = 1.0;
    return;
}

// Add a BBox to the BoxVec map
void TrackedObjectBBox::AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height, float _angle)
{
    // Check if the given frame number is valid
    if (_frame_num < 0)
        return;

    // Instantiate a new bounding-box
    BBox newBBox = BBox(_cx, _cy, _width, _height, _angle);

    // Get the time of given frame
    double time = this->FrameNToTime(_frame_num, 1.0);
    // Create an iterator that points to the BoxVec pair indexed by the time of given frame
    auto BBoxIterator = BoxVec.find(time);
    
    if (BBoxIterator != BoxVec.end())
    {
        // There is a bounding-box indexed by the time of given frame, update-it
        BBoxIterator->second = newBBox;
    }
    else
    {
        // There isn't a bounding-box indexed by the time of given frame, insert a new one
        BoxVec.insert({time, newBBox});
    }
}

// Get the size of BoxVec map
int64_t TrackedObjectBBox::GetLength() const
{
    if (BoxVec.empty())
        return 0;
    if (BoxVec.size() == 1)
        return 1;
    return BoxVec.size();
}

// Check if there is a bounding-box in the given frame
bool TrackedObjectBBox::Contains(int64_t frame_num)
{
    // Get the time of given frame
    double time = this->FrameNToTime(frame_num, 1.0);
    // Create an iterator that points to the BoxVec pair indexed by the time of given frame (or the closest time)
    auto it = BoxVec.lower_bound(time);
    if (it == BoxVec.end())
        // BoxVec pair not found
        return false;
    return true;
}

// Remove a bounding-box from the BoxVec map
void TrackedObjectBBox::RemoveBox(int64_t frame_number)
{
    // Get the time of given frame
    double time = this->FrameNToTime(frame_number, 1.0);
    // Create an iterator that points to the BoxVec pair indexed by the time of given frame
    auto it = BoxVec.find(time);
    if (it != BoxVec.end())
    {
        // The BoxVec pair exists, so remove it
        BoxVec.erase(time);
    }
    return;
}

// Return a bounding-box from BoxVec with it's properties adjusted by the Keyframes
BBox TrackedObjectBBox::GetBox(int64_t frame_number)
{
    // Get the time position of the given frame.
    double time = this->FrameNToTime(frame_number, this->TimeScale);

    // Return a iterator pointing to the BoxVec pair indexed by time or to the pair indexed
    // by the closest upper time value.  
    auto currentBBoxIterator = BoxVec.lower_bound(time);

    // Check if there is a pair indexed by time, returns an empty bbox if there isn't.
    if (currentBBoxIterator == BoxVec.end())
    {
        // Create and return an empty bounding-box object
        BBox emptyBBox;
        return emptyBBox;
    }

    // Check if the iterator matches a BBox indexed by time or points to the first element of BoxVec
    if ((currentBBoxIterator->first == time) || (currentBBoxIterator == BoxVec.begin()))
    {
        // Get the BBox indexed by time
        BBox currentBBox = currentBBoxIterator->second;

        // Adjust the BBox properties by the Keyframes values
        currentBBox.cx += this->delta_x.GetValue(frame_number);
        currentBBox.cy += this->delta_y.GetValue(frame_number);
        currentBBox.width *= this->scale_x.GetValue(frame_number);
        currentBBox.height *= this->scale_y.GetValue(frame_number);
        currentBBox.angle += this->rotation.GetValue(frame_number);
    
        return currentBBox;
    }

    // BBox indexed by the closest upper time
    BBox currentBBox = currentBBoxIterator->second;
    // BBox indexed by the closet lower time
    BBox previousBBox = prev(currentBBoxIterator, 1)->second;

    // Interpolate a BBox in the middle of previousBBox and currentBBox
    BBox interpolatedBBox = InterpolateBoxes(prev(currentBBoxIterator, 1)->first, currentBBoxIterator->first,
                                             previousBBox, currentBBox, time);

    // Adjust the BBox properties by the Keyframes values
    interpolatedBBox.cx += this->delta_x.GetValue(frame_number);
    interpolatedBBox.cy += this->delta_y.GetValue(frame_number);
    interpolatedBBox.width *= this->scale_x.GetValue(frame_number);
    interpolatedBBox.height *= this->scale_y.GetValue(frame_number);
    interpolatedBBox.angle += this->rotation.GetValue(frame_number);
    
    return interpolatedBBox;
}

// Interpolate the bouding-boxes properties
BBox TrackedObjectBBox::InterpolateBoxes(double t1, double t2, BBox left, BBox right, double target)
{
    // Interpolate the x-coordinate of the center point
    Point cx_left(t1, left.cx, openshot::InterpolationType::LINEAR);
    Point cx_right(t2, right.cx, openshot::InterpolationType::LINEAR);
    Point cx = InterpolateBetween(cx_left, cx_right, target, 0.01);

    // Interpolate de y-coordinate of the center point
    Point cy_left(t1, left.cy, openshot::InterpolationType::LINEAR);
    Point cy_right(t2, right.cy, openshot::InterpolationType::LINEAR);
    Point cy = InterpolateBetween(cy_left, cy_right, target, 0.01);

    // Interpolate the width
    Point width_left(t1, left.width, openshot::InterpolationType::LINEAR);
    Point width_right(t2, right.width, openshot::InterpolationType::LINEAR);
    Point width = InterpolateBetween(width_left, width_right, target, 0.01);

    // Interpolate the height
    Point height_left(t1, left.height, openshot::InterpolationType::LINEAR);
    Point height_right(t2, right.height, openshot::InterpolationType::LINEAR);
    Point height = InterpolateBetween(height_left, height_right, target, 0.01);

    // Interpolate the rotation angle
    Point angle_left(t1, left.angle, openshot::InterpolationType::LINEAR);
    Point angle_right(t1, right.angle, openshot::InterpolationType::LINEAR);
    Point angle = InterpolateBetween(angle_left, angle_right, target, 0.01);

    // Create a bounding box with the interpolated points
    BBox interpolatedBox(cx.co.Y, cy.co.Y, width.co.Y, height.co.Y, angle.co.Y);

    return interpolatedBox;
}

// Update object's BaseFps
void TrackedObjectBBox::SetBaseFPS(Fraction fps){
    this->BaseFps = fps;
    return;
}

// Return the object's BaseFps
Fraction TrackedObjectBBox::GetBaseFPS(){
    return BaseFps;
}

// Get the time of the given frame
double TrackedObjectBBox::FrameNToTime(int64_t frame_number, double time_scale){
    double time = ((double)frame_number) * this->BaseFps.Reciprocal().ToDouble() * (1.0 / time_scale);

    return time;
}

// Update the TimeScale member variable
void TrackedObjectBBox::ScalePoints(double time_scale){
    this->TimeScale = time_scale;
}

// Load the bounding-boxes information from the protobuf file 
bool TrackedObjectBBox::LoadBoxData(std::string inputFilePath)
{
    // Variable to hold the loaded data
    pb_tracker::Tracker bboxMessage;

    // Read the existing tracker message.
    fstream input(inputFilePath, ios::in | ios::binary);

    //Check if it was able to read the protobuf data
    if (!bboxMessage.ParseFromIstream(&input))
    {
        cerr << "Failed to parse protobuf message." << endl;
        return false;
    }
    
    this->clear();

    // Iterate over all frames of the saved message
    for (size_t i = 0; i < bboxMessage.frame_size(); i++)
    {
        // Get data of the i-th frame
        const pb_tracker::Frame &pbFrameData = bboxMessage.frame(i);

        // Get frame number
        size_t frame_number = pbFrameData.id();

        // Get bounding box data from current frame
        const pb_tracker::Frame::Box &box = pbFrameData.bounding_box();

        float width = box.x2() - box.x1();
        float height = box.y2() - box.y1();
        float cx = box.x1() + width/2;
        float cy = box.y1() + height/2;
        float angle = 0.0;


        if ( (cx >= 0.0) && (cy >= 0.0) && (width >= 0.0) && (height >= 0.0) )
        {
            // The bounding-box properties are valid, so add it to the BoxVec map
            this->AddBox(frame_number, cx, cy, width, height, angle);
        }
    }
    
    // Show the time stamp from the last update in tracker data file
    if (bboxMessage.has_last_updated())
    {
        cout << " Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(bboxMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

// Clear the BoxVec map
void TrackedObjectBBox::clear()
{
    BoxVec.clear();
}

// Generate JSON string of this object
std::string TrackedObjectBBox::Json() const
{
    // Return formatted string
    return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value TrackedObjectBBox::JsonValue() const
{
    // Create root json object
    Json::Value root;

    // Object's properties
    root["box_id"] = Id();
    root["BaseFPS"]["num"] = BaseFps.num;
    root["BaseFPS"]["den"] = BaseFps.den;
    root["TimeScale"] = TimeScale;

    // Keyframe's properties
    root["delta_x"] = delta_x.JsonValue();
    root["delta_y"] = delta_y.JsonValue();
    root["scale_x"] = scale_x.JsonValue();
    root["scale_y"] = scale_y.JsonValue(); 
    root["rotation"] = rotation.JsonValue();

    // return JsonValue
    return root;
}

// Load JSON string into this object
void TrackedObjectBBox::SetJson(const std::string value)
{
    // Parse JSON string into JSON objects
    try
    {
        const Json::Value root = openshot::stringToJson(value);
        // Set all values that match
        SetJsonValue(root);
    }
    catch (const std::exception &e)
    {
        // Error parsing JSON (or missing keys)
        throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
    }
    return;
}

// Load Json::Value into this object
void TrackedObjectBBox::SetJsonValue(const Json::Value root)
{   

    // Set the Id
    if (!root["box_id"].isNull())
        Id(root["box_id"].asString());

    // Set the BaseFps by the given JSON object
    if (!root["BaseFPS"].isNull() && root["BaseFPS"].isObject())
    {      
        if (!root["BaseFPS"]["num"].isNull())
            BaseFps.num = (int)root["BaseFPS"]["num"].asInt();
        if (!root["BaseFPS"]["den"].isNull())
            BaseFps.den = (int)root["BaseFPS"]["den"].asInt();
    }

    // Set the TimeScale by the given JSON object
    if (!root["TimeScale"].isNull())
    {
        double scale = (double)root["TimeScale"].asDouble();
        this->ScalePoints(scale);
    }

    if (!root["protobuf_data_path"].isNull())
        protobufDataPath = root["protobuf_data_path"].asString();

    // Set the Keyframes by the given JSON object
    if (!root["delta_x"].isNull())
        delta_x.SetJsonValue(root["delta_x"]);
    if (!root["delta_y"].isNull())
		delta_y.SetJsonValue(root["delta_y"]);
    if (!root["scale_x"].isNull())
        scale_x.SetJsonValue(root["scale_x"]);
    if (!root["scale_y"].isNull())
        scale_y.SetJsonValue(root["scale_y"]);
    if (!root["rotation"].isNull())
        rotation.SetJsonValue(root["rotation"]);

    return;
}

// Get all properties for a specific frame (perfect for a UI to display the current state
// of all properties at any time)
Json::Value TrackedObjectBBox::PropertiesJSON(int64_t requested_frame) const
{
    Json::Value root;

    BBox box = GetBox(requested_frame);

    // Id
    root["box_id"] = add_property_json("Box ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);

    // Add the data of given frame bounding-box to the JSON object
	root["x1"] = add_property_json("X1", box.cx-(box.width/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["y1"] = add_property_json("Y1", box.cy-(box.height/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["x2"] = add_property_json("X2", box.cx+(box.width/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["y2"] = add_property_json("Y2", box.cy+(box.height/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);

	// Add the bounding-box Keyframes to the JSON object
	root["delta_x"] = add_property_json("Displacement X-axis", delta_x.GetValue(requested_frame), "float", "", &delta_x, -1.0, 1.0, false, requested_frame);
	root["delta_y"] = add_property_json("Displacement Y-axis", delta_y.GetValue(requested_frame), "float", "", &delta_y, -1.0, 1.0, false, requested_frame);
	root["scale_x"] = add_property_json("Scale (Width)", scale_x.GetValue(requested_frame), "float", "", &scale_x, -1.0, 1.0, false, requested_frame);
	root["scale_y"] = add_property_json("Scale (Height)", scale_y.GetValue(requested_frame), "float", "", &scale_y, -1.0, 1.0, false, requested_frame);
	root["rotation"] = add_property_json("Rotation", rotation.GetValue(requested_frame), "float", "", &rotation, 0, 360, false, requested_frame);

	// Return formatted string
	return root;
}


// Generate JSON for a property
Json::Value TrackedObjectBBox::add_property_json(std::string name, float value, std::string type, std::string memo, const Keyframe* keyframe, float min_value, float max_value, bool readonly, int64_t requested_frame) const {

	// Requested Point
	const Point requested_point(requested_frame, requested_frame);

	// Create JSON Object
	Json::Value prop = Json::Value(Json::objectValue);
	prop["name"] = name;
	prop["value"] = value;
	prop["memo"] = memo;
	prop["type"] = type;
	prop["min"] = min_value;
	prop["max"] = max_value;
	if (keyframe) {
		prop["keyframe"] = keyframe->Contains(requested_point);
		prop["points"] = int(keyframe->GetCount());
		Point closest_point = keyframe->GetClosestPoint(requested_point);
		prop["interpolation"] = closest_point.interpolation;
		prop["closest_point_x"] = closest_point.co.X;
		prop["previous_point_x"] = keyframe->GetPreviousPoint(closest_point).co.X;
	}
	else {
		prop["keyframe"] = false;
		prop["points"] = 0;
		prop["interpolation"] = CONSTANT;
		prop["closest_point_x"] = -1;
		prop["previous_point_x"] = -1;
	}

	prop["readonly"] = readonly;
	prop["choices"] = Json::Value(Json::arrayValue);

	// return JsonValue
	return prop;
}

// Return the bounding box properties and it's keyframes indexed by their names
std::map<std::string, float> TrackedObjectBBox::GetBoxValues(int64_t frame_number){

    // Create the map
    std::map<std::string, float> boxValues;

    // Get bounding box of the current frame
    BBox box = GetBox(frame_number);

    // Save the bounding box properties
    boxValues["cx"] = box.cx;
    boxValues["cy"] = box.cy;
    boxValues["w"] = box.width;
    boxValues["h"] = box.height;
    boxValues["ang"] = box.angle;

    // Save the keyframes values
    boxValues["sx"] = this->scale_x.GetValue(frame_number);
    boxValues["sy"] = this->scale_y.GetValue(frame_number);
    boxValues["dx"] = this->delta_x.GetValue(frame_number);
    boxValues["dy"] = this->delta_y.GetValue(frame_number);
    boxValues["r"] = this->rotation.GetValue(frame_number);

    
    return boxValues;
}

// Return properties of this object's parent clip
std::map<std::string, float> TrackedObjectBBox::GetParentClipProperties(int64_t frame_number){
    
    // Get the parent clip of this object as a Clip pointer
    Clip* parentClip = (Clip *) ParentClip();

    // Calculate parentClip's frame number
    long parentClip_start_position = round( parentClip->Position() * parentClip->info.fps.ToDouble() ) + 1;
    long parentClip_start_frame = ( parentClip->Start() * parentClip->info.fps.ToDouble() ) + 1;
    float parentClip_frame_number = round(frame_number - parentClip_start_position) + parentClip_start_frame;

    // Get parentClip's Keyframes
    float parentClip_location_x = parentClip->location_x.GetValue(parentClip_frame_number);
    float parentClip_location_y = parentClip->location_y.GetValue(parentClip_frame_number);
    float parentClip_scale_x = parentClip->scale_x.GetValue(parentClip_frame_number);
    float parentClip_scale_y = parentClip->scale_y.GetValue(parentClip_frame_number);
    float parentClip_rotation = parentClip->rotation.GetValue(parentClip_frame_number);

    std::map<std::string, float> parentClipProperties;

    parentClipProperties["frame_number"] = parentClip_frame_number;
    parentClipProperties["timeline_frame_number"] = frame_number;
    parentClipProperties["location_x"] = parentClip_location_x;
    parentClipProperties["location_y"] = parentClip_location_y;
    parentClipProperties["scale_x"] = parentClip_scale_x;
    parentClipProperties["scale_y"] = parentClip_scale_y;
    parentClipProperties["rotation"] = parentClip_rotation;

    return parentClipProperties;
}