#include "generator/opentable_dataset.hpp"

#include "generator/feature_builder.hpp"
#include "generator/sponsored_scoring.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/string_utils.hpp"

#include "std/iomanip.hpp"

#include "boost/algorithm/string/replace.hpp"

namespace generator
{
// OpentableRestaurant ------------------------------------------------------------------------------
OpentableRestaurant::OpentableRestaurant(string const & src)
{
  vector<string> rec;
  strings::ParseCSVRow(src, '\t', rec);
  CHECK_EQUAL(rec.size(), FieldsCount(), ("Error parsing restaurants.tsv line:",
                                          boost::replace_all_copy(src, "\t", "\\t")));

  strings::to_uint(rec[FieldIndex(Fields::Id)], m_id.Get());
  strings::to_double(rec[FieldIndex(Fields::Latitude)], m_latLon.lat);
  strings::to_double(rec[FieldIndex(Fields::Longtitude)], m_latLon.lon);

  m_name = rec[FieldIndex(Fields::Name)];
  m_address = rec[FieldIndex(Fields::Address)];
  m_descUrl = rec[FieldIndex(Fields::DescUrl)];
}

ostream & operator<<(ostream & s, OpentableRestaurant const & h)
{
  s << fixed << setprecision(7);
  return s << "Id: " << h.m_id << "\t Name: " << h.m_name << "\t Address: " << h.m_address
           << "\t lat: " << h.m_latLon.lat << " lon: " << h.m_latLon.lon;
}

// OpentableDataset ---------------------------------------------------------------------------------
template <>
bool OpentableDataset::NecessaryMatchingConditionHolds(FeatureBuilder1 const & fb) const
{
  if (fb.GetName(StringUtf8Multilang::kDefaultCode).empty())
    return false;

  return ftypes::IsFoodChecker::Instance()(fb.GetTypes());
}

template <>
void OpentableDataset::PreprocessMatchedOsmObject(ObjectId const matchedObjId, FeatureBuilder1 & fb,
                                                  function<void(FeatureBuilder1 &)> const fn) const
{
  FeatureParams params = fb.GetParams();

  auto restaurant = GetObjectById(matchedObjId);
  auto & metadata = params.GetMetadata();
  metadata.Set(feature::Metadata::FMD_SPONSORED_ID, strings::to_string(restaurant.m_id.Get()));

  // params.AddAddress(restaurant.address);
  // TODO(mgsergio): addr:full ???

  params.AddName(StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kDefaultCode),
                 restaurant.m_name);

  auto const & clf = classif();
  params.AddType(clf.GetTypeByPath({"sponsored", "opentable"}));

  fb.SetParams(params);

  fn(fb);
}

template <>
OpentableDataset::ObjectId OpentableDataset::FindMatchingObjectIdImpl(FeatureBuilder1 const & fb) const
{
  auto const name = fb.GetName(StringUtf8Multilang::kDefaultCode);

  if (name.empty())
    return Object::InvalidObjectId();

  // Find |kMaxSelectedElements| nearest values to a point.
  auto const nearbyIds = GetNearestObjects(MercatorBounds::ToLatLon(fb.GetKeyPoint()),
                                           kMaxSelectedElements, kDistanceLimitInMeters);

  for (auto const objId : nearbyIds)
  {
    if (sponsored_scoring::Match(GetObjectById(objId), fb).IsMatched())
      return objId;
  }

  return Object::InvalidObjectId();
}
}  // namespace generator
