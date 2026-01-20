#pragma once

#include "pch.h"
#include "Camera.h"
#include "Models.h"
#include "WallType.h"
#include "ViewSettings.h"
#include <vector>
#include <cmath>

namespace winrt::estimate1
{
    // Forward declarations
    class WallJoinManager;

    // Represents a single polyline in world coordinates
    struct WorldPolyline
    {
        std::vector<WorldPoint> Points;
        bool IsClosed{ false };

        WorldPolyline() = default;
        
        void AddPoint(const WorldPoint& pt) { Points.push_back(pt); }
        
        bool IsEmpty() const { return Points.size() < 2; }
        
        void Clear() { Points.clear(); }
    };

    // Wall plan geometry - the computed representation for rendering
    // Contains world-space polylines for wall boundaries
    struct WallPlanGeometry
    {
        uint64_t WallId{ 0 };
        
        // Single closed boundary path (world coordinates)
        // This forms the "static contour" of the wall
        WorldPolyline BoundaryPath;
        
        // Internal layer boundaries (Fine detail level only)
        std::vector<WorldPolyline> LayerBoundaries;
        
        // Join status for ends
        bool StartJoined{ false };
        bool EndJoined{ false };
        
        // Cached hash for dirty checking
        size_t GeometryHash{ 0 };
        
        bool IsValid() const 
        { 
            return !BoundaryPath.IsEmpty(); 
        }

        void Clear()
        {
            BoundaryPath.Clear();
            LayerBoundaries.clear();
            StartJoined = false;
            EndJoined = false;
            GeometryHash = 0;
        }
    };

    // Builder that computes wall plan geometry in world space
    class WallPlanGeometryBuilder
    {
    public:
        WallPlanGeometryBuilder() = default;

        // Build plan geometry for a single wall
        // This creates a "static contour" - a stable geometric representation in world space
        WallPlanGeometry BuildBasic(const Wall& wall, DetailLevel detailLevel = DetailLevel::Medium) const
        {
            WallPlanGeometry result;
            result.WallId = wall.GetId();

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();
            double thickness = (std::max)(50.0, wall.GetThickness());
            
            // Compute wall direction and perpendicular
            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double length = std::sqrt(dx * dx + dy * dy);
            
            if (length < 0.1)
            {
                return result; // Too short
            }
            
            // Normalized direction and perpendicular
            WorldPoint dir(dx / length, dy / length);
            WorldPoint perp(-dir.Y, dir.X);
            
            // Compute offset based on location line mode
            double locationOffset = ComputeLocationLineOffset(wall, perp);
            
            // Axis points (center of the wall considering location line)
            WorldPoint startAxis(start.X + perp.X * locationOffset, start.Y + perp.Y * locationOffset);
            WorldPoint endAxis(end.X + perp.X * locationOffset, end.Y + perp.Y * locationOffset);
            
            // Half thickness for boundaries
            double halfThickness = thickness / 2.0;
            
            // 4 Corner points of the wall "static contour"
            WorldPoint p1(startAxis.X + perp.X * halfThickness, startAxis.Y + perp.Y * halfThickness);
            WorldPoint p2(startAxis.X - perp.X * halfThickness, startAxis.Y - perp.Y * halfThickness);
            WorldPoint p3(endAxis.X - perp.X * halfThickness, endAxis.Y - perp.Y * halfThickness);
            WorldPoint p4(endAxis.X + perp.X * halfThickness, endAxis.Y + perp.Y * halfThickness);
            
            // Build the closed loop: start-left -> end-left -> end-right -> start-right
            result.BoundaryPath.AddPoint(p1);
            result.BoundaryPath.AddPoint(p4);
            result.BoundaryPath.AddPoint(p3);
            result.BoundaryPath.AddPoint(p2);
            result.BoundaryPath.IsClosed = true;
            
            // Layer boundaries for Fine detail
            if (detailLevel == DetailLevel::Fine)
            {
                BuildLayerBoundaries(wall, startAxis, endAxis, perp, halfThickness, result.LayerBoundaries);
            }
            
            // Compute geometry hash for caching
            result.GeometryHash = ComputeHash(wall);
            
            return result;
        }

        // Check if cached geometry is still valid
        bool IsCacheValid(const Wall& wall, const WallPlanGeometry& cached) const
        {
            return cached.WallId == wall.GetId() && 
                   cached.GeometryHash == ComputeHash(wall) &&
                   cached.IsValid();
        }

    private:
        // Compute offset from wall axis based on location line mode
        double ComputeLocationLineOffset(const Wall& wall, const WorldPoint& /*perp*/) const
        {
            double thickness = wall.GetThickness();
            double halfThickness = thickness / 2.0;
            
            // Core thickness computation
            double coreThickness = thickness;
            if (auto type = wall.GetType())
            {
                coreThickness = type->GetCoreThickness();
                if (coreThickness <= 0.0) coreThickness = thickness;
                coreThickness = (std::min)(coreThickness, thickness);
            }
            double finishThickness = (std::max)(0.0, thickness - coreThickness);
            
            switch (wall.GetLocationLineMode())
            {
            case LocationLineMode::WallCenterline:
            case LocationLineMode::CoreCenterline:
                return 0.0;
                
            case LocationLineMode::FinishFaceExterior:
                return halfThickness;
                
            case LocationLineMode::CoreFaceExterior:
                return coreThickness / 2.0 + finishThickness;
                
            case LocationLineMode::FinishFaceInterior:
                return -halfThickness;
                
            case LocationLineMode::CoreFaceInterior:
                return -(coreThickness / 2.0 + finishThickness);
                
            default:
                return 0.0;
            }
        }

        // Build layer boundaries for compound walls
        void BuildLayerBoundaries(
            const Wall& wall,
            const WorldPoint& startAxis,
            const WorldPoint& endAxis,
            const WorldPoint& perp,
            double halfThickness,
            std::vector<WorldPolyline>& layerBoundaries) const
        {
            auto type = wall.GetType();
            if (!type || type->GetLayerCount() <= 1)
                return;
            
            const auto& layers = type->GetLayers();
            double cumulativeOffset = halfThickness;
            
            for (size_t i = 0; i < layers.size() - 1; ++i)
            {
                cumulativeOffset -= layers[i].Thickness;
                
                WorldPolyline boundary;
                boundary.AddPoint(WorldPoint(
                    startAxis.X + perp.X * cumulativeOffset,
                    startAxis.Y + perp.Y * cumulativeOffset));
                boundary.AddPoint(WorldPoint(
                    endAxis.X + perp.X * cumulativeOffset,
                    endAxis.Y + perp.Y * cumulativeOffset));
                
                layerBoundaries.push_back(std::move(boundary));
            }
        }

        // Compute a hash for dirty checking
        size_t ComputeHash(const Wall& wall) const
        {
            size_t hash = 0;
            
            // Simple hash combining
            auto hashCombine = [&hash](double value) {
                size_t bits = *reinterpret_cast<size_t*>(&value);
                hash ^= bits + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            };
            
            hashCombine(wall.GetStartPoint().X);
            hashCombine(wall.GetStartPoint().Y);
            hashCombine(wall.GetEndPoint().X);
            hashCombine(wall.GetEndPoint().Y);
            hashCombine(wall.GetThickness());
            hash ^= static_cast<size_t>(wall.GetLocationLineMode());
            
            return hash;
        }
    };
}
