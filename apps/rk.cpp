#include <cmath>
#include <iostream>
#include <vector>
#include <filesystem>
#include <list>

#include "tinyxml2.h"
#include "tz.h"
#include "date.h"

namespace fs = std::filesystem;

std::string formatDuration(double duration)
{
    int hours = duration / 3600;
    int minutes = (duration - hours * 3600) / 60;
    int seconds = duration - hours * 3600 - minutes * 60;
    std::ostringstream ost;
    if (hours) ost << hours << ":";
    if (minutes==0) {ost << "00:";} else if (minutes<10) {ost << "0" << minutes << ":";} else {ost << minutes << ":";}
    if (seconds<10) {ost << "0";} ost << seconds;
    return ost.str();
}

std::string formatPace(double pace)
{
    int minutes = pace / 60;
    int seconds = pace - minutes * 60;
    
    std::ostringstream ost;
    ost << minutes << ":";
    if (seconds < 10) ost << "0";
    ost << seconds;
    return ost.str();
}

double deg2rad(double deg)
{
    return (deg * M_PI) / 180.0;
}

double rad2deg(double rad)
{
    return (rad * 180.0) / M_PI;
}

// http://www.movable-type.co.uk/scripts/latlong.html

// distance in metres
double calcDistance(double lat1deg, double lon1deg, double lat2deg, double lon2deg)
{
    const double R = 6371E3; // meters

    double lat1rad = deg2rad(lat1deg);
    double lon1rad = deg2rad(lon1deg);
    double lat2rad = deg2rad(lat2deg);
    double lon2rad = deg2rad(lon2deg);

    double dlat = lat2rad - lat1rad;
    double dlon = lon2rad - lon1rad;

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) + cos(lat1rad) * cos(lat2rad) * sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return c * R;
}

class Activity
{
public:
    bool load(const char* filename)
    {
        tinyxml2::XMLDocument doc;

        duration_ = duration_.zero();

        tinyxml2::XMLError result = doc.LoadFile(filename);
        if (result != tinyxml2::XML_SUCCESS)
        {
            std::cout << "Failed to load file: " << filename << ", error: " << result << std::endl;
            return false;
        }

        tinyxml2::XMLElement* gpx = doc.RootElement();
        tinyxml2::XMLElement* trk = gpx->FirstChildElement();
        tinyxml2::XMLElement* trkName = trk->FirstChildElement("name");
        std::string name = trkName->GetText();
        
        std::size_t pos = name.find_first_of("0123456789");
        if (pos == std::string::npos || pos == 0)
        {
            std::cout << "Strange data in the track's name field: '" << name << "' in file: " << filename << result << std::endl;
            return false;
        }
        activityType_ = name.substr(0, pos - 1);

        tinyxml2::XMLElement* trkTime = trk->FirstChildElement("time");
        startTime_ = trkTime->GetText();

        nofSegs_ = 0;
        distance_ = 0;
        tinyxml2::XMLElement* trkSeg = trk->FirstChildElement("trkseg");
        while (trkSeg)
        {
            nofSegs_++;
            int64_t maxi = 0;
            int64_t mini = 0x7FFFFFFFFFFFFFFF;
            double lat0 = 0;
            double lon0 = 0;
            double segDistance = 0;
            tinyxml2::XMLElement* trkPt = trkSeg->FirstChildElement("trkpt");
            if (trkPt)
            {
                auto err = trkPt->QueryDoubleAttribute("lat", &lat0);
                if (err) {std::cout << "lat err" << std::endl; return false; }
                err = trkPt->QueryDoubleAttribute("lon", &lon0);
                if (err) {std::cout << "lon err" << std::endl; return false; }
            }
            while (trkPt)
            {
                double lat1, lon1;                
                auto err = trkPt->QueryDoubleAttribute("lat", &lat1);
                if (err) {std::cout << "lat err" << std::endl; return false; }
                err = trkPt->QueryDoubleAttribute("lon", &lon1);
                if (err) {std::cout << "lon err" << std::endl; return false; }

                segDistance += calcDistance(lat0, lon0, lat1, lon1);
                lat0 = lat1;
                lon0 = lon1;

                //auto ele = trkPt->FirstChildElement("ele");
                //double alt = ele->DoubleText();

                date::utc_seconds timestamp;
                auto time = trkPt->FirstChildElement("time");
                std::istringstream ist(time->GetText());
                ist >> date::parse("%FT%TZ", timestamp);
                if (timestamp.time_since_epoch().count() < mini) mini = timestamp.time_since_epoch().count();
                if (timestamp.time_since_epoch().count() > maxi) maxi = timestamp.time_since_epoch().count();

                trkPt = trkPt->NextSiblingElement("trkpt");
            }
            duration_ += static_cast<std::chrono::seconds>(maxi - mini);
            distance_ += segDistance;
            trkSeg = trkSeg->NextSiblingElement("trkseg");
        }
        if (distance_ == 0)
        {
            std::cout << "Error, no distance in file: " << filename << std::endl;
            return false;
        }
        paceSecsPerKm_ = duration_.count() / (distance_ / 1000.0); // Seconds per kilometer
        return true;
    }
    double distance() const
    {
        return distance_;
    }

    double duration() const
    {
        return duration_.count();
    }

    double pace() const
    {
        return paceSecsPerKm_;
    }

    std::string startTime() const
    {
        return startTime_;
    }

    std::string type() const
    {
        return activityType_;
    }

    int numberOfSegments() const
    {
        return nofSegs_;
    }

private:
    std::string activityType_;
    std::string startTime_;
    date::utc_seconds::duration duration_;
    double distance_;
    double paceSecsPerKm_;
    int nofSegs_;
};

int main(int argc, char* argv[])
{
    std::list<Activity> running;
    std::list<Activity> walking;

    const fs::path pathToShow{ argc >= 2 ? argv[1] : fs::current_path() };

    auto sorter = [](const Activity& l, const Activity& r) {
        int ld = l.distance() / 1000;
        int rd = r.distance() / 1000;
        int lp = l.pace();
        int rp = r.pace();
        if (ld < rd)
        {
            return true;
        }
        else if (ld == rd && lp < rp)
        {
            return true;
        }
        return false;
    };

    for (const auto& entry : fs::directory_iterator(pathToShow)) 
    {
        if (!entry.is_regular_file() || entry.path().filename().extension() != ".gpx")
        {
            continue;
        }
        const std::string filePath = entry.path().string();
        Activity activity;
        if (activity.load(filePath.c_str()))
        {
            std::cout << "."; std::cout.flush();
            if (activity.type() == "Running")
            {
                std::list<Activity>::iterator it = std::lower_bound(running.begin(), running.end(), activity, sorter);
                running.insert(it, activity);
            }
            else if (activity.type() == "Walking")
            {
                std::list<Activity>::iterator it = std::lower_bound(walking.begin(), walking.end(), activity, sorter);
                walking.insert(it, activity);
            }
            /*
            std:: cout  << "File: " << entry.path().filename()
                        << ", type: " << activity.type() 
                        << ", started: " << activity.startTime() 
                        << ", distance: " << activity.distance() 
                        << ", duration: " << activity.duration() 
                        << ", " << formatDuration(activity.duration()) 
                        << ", pace: " << formatPace(activity.pace())
                        << ", segments: " << activity.numberOfSegments() 
                        << std::endl;
            */
        }
    }
    std::cout << "\n";
    std::cout << "\n================ WALKING " << walking.size() << " ================" << std::endl;
    for (auto& w : walking)
    {
        std::cout << w.startTime() << ", " << w.distance() << ", " << formatPace(w.pace()) << ", " << formatDuration(w.duration()) << std::endl; 
    }

    std::cout << "\n================ RUNNING " << running.size() << " ================" << std::endl;
    for (auto& r : running)
    {
        std::cout << r.startTime() << ", " << r.distance() << ", " << formatPace(r.pace()) << ", " << formatDuration(r.duration()) << std::endl; 
    }
}