/** @file
    @brief Header

    @date 2014

    @author
    Ryan Pavlik
    <ryan@sensics.com>
    <http://sensics.com>
*/

// Copyright 2014 Sensics, Inc.
//
// All rights reserved.
//
// (Final version intended to be licensed under
// the Apache License, Version 2.0)

#ifndef INCLUDED_VRPNTrackerRouter_h_GUID_C1F1095D_9483_4DB0_7A7A_9F82837E6C9C
#define INCLUDED_VRPNTrackerRouter_h_GUID_C1F1095D_9483_4DB0_7A7A_9F82837E6C9C

// Internal Includes
#include "VRPNContext.h"
#include <osvr/Util/QuatlibInteropC.h>
#include <osvr/Util/UniquePtr.h>
#include <osvr/Client/ClientContext.h>
#include <osvr/Client/ClientInterface.h>
#include <osvr/Transform/Transform.h>

// Library/third-party includes
#include <vrpn_Tracker.h>
#include <boost/optional.hpp>

// Standard includes
// - none

namespace osvr {
namespace client {
    class VRPNTrackerRouter : public RouterEntry {
      public:
        VRPNTrackerRouter(ClientContext *ctx, vrpn_Connection *conn,
                          const char *src, boost::optional<int> sensor,
                          const char *dest, transform::Transform const &t)
            : RouterEntry(ctx, dest),
              m_remote(new vrpn_Tracker_Remote(src, conn)), m_transform(t) {
            m_remote->register_change_handler(this, &VRPNTrackerRouter::handle,
                                              sensor.get_value_or(-1));
            m_remote->shutup = true;
        }

        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        static void VRPN_CALLBACK handle(void *userdata, vrpn_TRACKERCB info) {
            VRPNTrackerRouter *self =
                static_cast<VRPNTrackerRouter *>(userdata);
            OSVR_PoseReport report;
            report.sensor = info.sensor;
            OSVR_TimeValue timestamp;
            osvrStructTimevalToTimeValue(&timestamp, &(info.msg_time));
            osvrQuatFromQuatlib(&(report.pose.rotation), info.quat);
            osvrVec3FromQuatlib(&(report.pose.translation), info.pos);
            Eigen::Matrix4d pose = self->m_transform.transform(
                util::fromPose(report.pose).matrix());
            util::toPose(pose, report.pose);

            for (auto const &iface : self->getContext()->getInterfaces()) {
                if (iface->getPath() == self->getDest()) {
                    iface->triggerCallbacks(timestamp, report);
                }
            }

            /// @todo current heuristic for "do we have position data?" is
            /// "is our position non-zero?"
            if (util::vecMap(report.pose.translation) !=
                Eigen::Vector3d::Zero()) {
                OSVR_PositionReport positionReport;
                positionReport.sensor = info.sensor;
                positionReport.xyz = report.pose.translation;
                for (auto const &iface : self->getContext()->getInterfaces()) {
                    if (iface->getPath() == self->getDest()) {
                        iface->triggerCallbacks(timestamp, positionReport);
                    }
                }
            }

            /// @todo check to see if rotation is useful/provided
            {
                OSVR_OrientationReport oriReport;
                oriReport.sensor = info.sensor;
                oriReport.rotation = report.pose.rotation;
                for (auto const &iface : self->getContext()->getInterfaces()) {
                    if (iface->getPath() == self->getDest()) {
                        iface->triggerCallbacks(timestamp, oriReport);
                    }
                }
            }
        }
        void operator()() { m_remote->mainloop(); }

      private:
        unique_ptr<vrpn_Tracker_Remote> m_remote;
        transform::Transform m_transform;
    };

} // namespace client
} // namespace osvr

#endif // INCLUDED_VRPNTrackerRouter_h_GUID_C1F1095D_9483_4DB0_7A7A_9F82837E6C9C
