#include "routing/router.hpp"

static inline routing::RouterType coreRouterType(MWMRouterType type)
{
  switch (type)
  {
  case MWMRouterTypeVehicle: return routing::RouterType::Vehicle;
  case MWMRouterTypePedestrian: return routing::RouterType::Pedestrian;
  case MWMRouterTypeBicycle: return routing::RouterType::Bicycle;
  case MWMRouterTypeTaxi: return routing::RouterType::Taxi;
  }
}

static inline MWMRouterType routerType(routing::RouterType type)
{
  switch (type)
  {
  case routing::RouterType::Vehicle: return MWMRouterTypeVehicle;
  case routing::RouterType::Pedestrian: return MWMRouterTypePedestrian;
  case routing::RouterType::Bicycle: return MWMRouterTypeBicycle;
  case routing::RouterType::Taxi: return MWMRouterTypeTaxi;
  }
}
