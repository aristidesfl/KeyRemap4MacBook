#include <IOKit/IOLib.h>

#include "EventOutputQueue.hpp"
#include "IOLogWrapper.hpp"
#include "PointingButtonToPointingButton.hpp"

namespace org_pqrs_KeyRemap4MacBook {
  namespace RemapFunc {
    PointingButtonToPointingButton::PointingButtonToPointingButton(void) : index_(0)
    {}

    PointingButtonToPointingButton::~PointingButtonToPointingButton(void)
    {}

    void
    PointingButtonToPointingButton::add(unsigned int datatype, unsigned int newval)
    {
      switch (datatype) {
        case BRIDGE_DATATYPE_POINTINGBUTTON:
        {
          switch (index_) {
            case 0:
              fromButton_.button = PointingButton(newval);
              break;
            default:
              toButtons_.push_back(PairPointingButtonFlags(PointingButton(newval)));
              break;
          }
          ++index_;

          break;
        }

        case BRIDGE_DATATYPE_FLAGS:
        {
          switch (index_) {
            case 0:
              IOLOG_ERROR("Invalid PointingButtonToPointingButton::add\n");
              break;
            case 1:
              fromButton_.flags = Flags(newval);
              break;
            default:
              if (! toButtons_.empty()) {
                toButtons_.back().flags = Flags(newval);
              }
              break;
          }
          break;
        }

        default:
          IOLOG_ERROR("PointingButtonToPointingButton::add invalid datatype:%d\n", datatype);
          break;
      }
    }

    bool
    PointingButtonToPointingButton::remap(RemapParams& remapParams)
    {
      Params_RelativePointerEventCallback* params = remapParams.paramsUnion.get_Params_RelativePointerEventCallback();
      if (! params) return false;

      // Considering mouse drag, we need temporary_decrease fromButton_.flags each event.
      //
      // Note:
      // For the following events when using Command_L+LeftClick to MiddleClick,
      // we need to use temporary_decrease/temporary_increase instead of decrease/increase.
      //
      // (1) Command_L down
      // (2) LeftClick down  -> MiddleClick
      // (3) Mouse drag      -> MiddleClick drag
      // (4) KeyCode::C down -> Command_L+C (not "C without modifier")
      // (5) LeftClick up
      // (6) Command_L up

      if (remapParams.isremapped) return false;
      bool isFromButton = fromkeychecker_.isFromPointingButton(*params, fromButton_.button, fromButton_.flags);
      if (! isFromButton && ! fromkeychecker_.isactive()) {
        return false;
      }
      remapParams.isremapped = true;

      // We consider it about Option_L+LeftClick to MiddleClick.
      // LeftClick generates the following events by ButtonDown and ButtonUp.
      //
      // (1) buttons == PointingButton::LEFT  (ButtonDown event)
      // (2) buttons == PointingButton::NONE  (ButtonUp event)
      //
      // We must cancel Option_L in both (1), (2).
      //
      // Attention: We need fire MiddleClick only at (1).

      // The temporary flags is not changed at pointing move events.
      // Therefore, we need to clear temporary flags here to avoid infinity decreasing of fromButton_.flags.
      FlagStatus::set();

      FlagStatus::temporary_decrease(fromButton_.flags);

      if (isFromButton) {
        if (params->ex_isbuttondown) {
          ButtonStatus::decrease(fromButton_.button);
          if (toButtons_.size() == 1) {
            ButtonStatus::increase(toButtons_[0].button);
          }

        } else {
          ButtonStatus::increase(fromButton_.button);
          if (toButtons_.size() == 1) {
            ButtonStatus::decrease(toButtons_[0].button);
          }
        }
      }

      // ----------------------------------------
      switch (toButtons_.size()) {
        case 0:
          break;

        case 1:
          FlagStatus::temporary_increase(toButtons_[0].flags);
          EventOutputQueue::FireRelativePointer::fire(ButtonStatus::makeButtons(), params->dx, params->dy);
          FlagStatus::temporary_decrease(toButtons_[0].flags);
          break;

        case 2:
        {
          if (! params->ex_isbuttondown) {
            EventOutputQueue::FireRelativePointer::fire(ButtonStatus::makeButtons(), params->dx, params->dy);
          } else {
            for (size_t i = 0; i < toButtons_.size(); ++i) {
              FlagStatus::temporary_increase(toButtons_[i].flags);

              ButtonStatus::increase(toButtons_[i].button);
              EventOutputQueue::FireRelativePointer::fire(ButtonStatus::makeButtons());
              ButtonStatus::decrease(toButtons_[i].button);
              EventOutputQueue::FireRelativePointer::fire(ButtonStatus::makeButtons());

              FlagStatus::temporary_decrease(toButtons_[i].flags);
            }
          }
          break;
        }
      }

      return true;
    }
  }
}
