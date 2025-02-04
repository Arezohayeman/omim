#include "drape/texture_manager.hpp"
#include "drape/symbols_texture.hpp"
#include "drape/font_texture.hpp"
#include "drape/static_texture.hpp"
#include "drape/stipple_pen_resource.hpp"
#include "drape/texture_of_colors.hpp"
#include "drape/glfunctions.hpp"
#include "drape/utils/glyph_usage_tracker.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/reader.hpp"

#include "base/logging.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include "std/vector.hpp"
#include "std/bind.hpp"

namespace dp
{

uint32_t const kMaxTextureSize = 1024;
uint32_t const kStippleTextureWidth = 512;
uint32_t const kMinStippleTextureHeight = 64;
uint32_t const kMinColorTextureSize = 32;
size_t const kInvalidGlyphGroup = numeric_limits<size_t>::max();

// number of glyphs (since 0) which will be in each texture
size_t const kDuplicatedGlyphsCount = 128;

uint32_t const kReservedPatterns = 10;
size_t const kReservedColors = 20;

float const kGlyphAreaMultiplier = 1.2f;
float const kGlyphAreaCoverage = 0.9f;

std::string const kDefaultSymbolsTexture = "symbols";
std::string const kSymbolTextures[] = { kDefaultSymbolsTexture, "symbols-ad" };
uint32_t const kDefaultSymbolsIndex = 0;

namespace
{

void MultilineTextToUniString(TextureManager::TMultilineText const & text, strings::UniString & outString)
{
  size_t cnt = 0;
  for (strings::UniString const & str : text)
    cnt += str.size();

  outString.clear();
  outString.reserve(cnt);
  for (strings::UniString const & str : text)
    outString.append(str.begin(), str.end());
}

string ReadFileToString(string const & filename)
{
  string result;
  try
  {
    ReaderPtr<Reader>(GetPlatform().GetReader(filename)).ReadAsString(result);
  }
  catch(RootException const & e)
  {
    LOG(LWARNING, ("Error reading file ", filename, " : ", e.what()));
    return "";
  }
  return result;
}

template <typename ToDo>
void ParseColorsList(string const & colorsFile, ToDo toDo)
{
  istringstream fin(ReadFileToString(colorsFile));
  while (true)
  {
    uint32_t color;
    fin >> color;
    if (!fin)
      break;

    toDo(dp::Extract(color));
  }
}

template <typename ToDo>
void ParsePatternsList(string const & patternsFile, ToDo toDo)
{
  strings::Tokenize(ReadFileToString(patternsFile), "\n", [&](string const & patternStr)
  {
    if (patternStr.empty())
      return;

    buffer_vector<double, 8> pattern;
    strings::Tokenize(patternStr, " ", [&](string const & token)
    {
      double d = 0.0;
      VERIFY(strings::to_double(token, d), ());
      pattern.push_back(d);
    });

    bool isValid = true;
    for (size_t i = 0; i < pattern.size(); i++)
    {
      if (fabs(pattern[i]) < 1e-5)
      {
        LOG(LWARNING, ("Pattern was skipped", patternStr));
        isValid = false;
        break;
      }
    }

    if (isValid)
      toDo(pattern);
  });
}

} // namespace

TextureManager::TextureManager()
  : m_maxTextureSize(0)
  , m_maxGlypsCount(0)
{
  m_nothingToUpload.test_and_set();
}

TextureManager::BaseRegion::BaseRegion()
  : m_info(nullptr)
  , m_texture(nullptr)
{}

bool TextureManager::BaseRegion::IsValid() const
{
  return m_info != nullptr && m_texture != nullptr;
}

void TextureManager::BaseRegion::SetResourceInfo(ref_ptr<Texture::ResourceInfo> info)
{
  m_info = info;
}

void TextureManager::BaseRegion::SetTexture(ref_ptr<Texture> texture)
{
  m_texture = texture;
}

m2::PointF TextureManager::BaseRegion::GetPixelSize() const
{
  if (!IsValid())
    return m2::PointF(0.0f, 0.0f);

  m2::RectF const & texRect = m_info->GetTexRect();
  return m2::PointF(texRect.SizeX() * m_texture->GetWidth(),
                    texRect.SizeY() * m_texture->GetHeight());
}

float TextureManager::BaseRegion::GetPixelHeight() const
{
  if (!IsValid())
    return 0.0f;

  return m_info->GetTexRect().SizeY() * m_texture->GetHeight();
}

m2::RectF const & TextureManager::BaseRegion::GetTexRect() const
{
  if (!IsValid())
  {
    static m2::RectF nilRect(0.0f, 0.0f, 0.0f, 0.0f);
    return nilRect;
  }

  return m_info->GetTexRect();
}

TextureManager::GlyphRegion::GlyphRegion()
  : BaseRegion()
{
}

float TextureManager::GlyphRegion::GetOffsetX() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_xOffset;
}

float TextureManager::GlyphRegion::GetOffsetY() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_yOffset;
}

float TextureManager::GlyphRegion::GetAdvanceX() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_xAdvance;
}

float TextureManager::GlyphRegion::GetAdvanceY() const
{
  ASSERT(m_info->GetType() == Texture::Glyph, ());
  return ref_ptr<GlyphInfo>(m_info)->GetMetrics().m_yAdvance;
}

uint32_t TextureManager::StippleRegion::GetMaskPixelLength() const
{
  ASSERT(m_info->GetType() == Texture::StipplePen, ());
  return ref_ptr<StipplePenResourceInfo>(m_info)->GetMaskPixelLength();
}

uint32_t TextureManager::StippleRegion::GetPatternPixelLength() const
{
  ASSERT(m_info->GetType() == Texture::StipplePen, ());
  return ref_ptr<StipplePenResourceInfo>(m_info)->GetPatternPixelLength();
}

void TextureManager::Release()
{
  m_glyphGroups.clear();
  m_hybridGlyphGroups.clear();

  m_symbolTextures.clear();
  m_stipplePenTexture.reset();
  m_colorTexture.reset();

  m_trafficArrowTexture.reset();

  m_glyphTextures.clear();

  m_glyphManager.reset();
}

bool TextureManager::UpdateDynamicTextures()
{
  bool const asyncRoutines = HasAsyncRoutines(m_glyphGroups) || HasAsyncRoutines(m_hybridGlyphGroups);

  if (!asyncRoutines && m_nothingToUpload.test_and_set())
    return false;

  m_colorTexture->UpdateState();
  m_stipplePenTexture->UpdateState();

  UpdateGlyphTextures(m_glyphGroups);
  UpdateGlyphTextures(m_hybridGlyphGroups);

  m_textureAllocator->Flush();

  return true;
}

ref_ptr<Texture> TextureManager::AllocateGlyphTexture()
{
  m2::PointU size(m_maxTextureSize, m_maxTextureSize);
  m_glyphTextures.push_back(make_unique_dp<FontTexture>(size, make_ref(m_glyphManager), make_ref(m_textureAllocator)));
  return make_ref(m_glyphTextures.back());
}

void TextureManager::GetRegionBase(ref_ptr<Texture> tex, TextureManager::BaseRegion & region, Texture::Key const & key)
{
  bool isNew = false;
  region.SetResourceInfo(tex != nullptr ? tex->FindResource(key, isNew) : nullptr);
  region.SetTexture(tex);
  ASSERT(region.IsValid(), ());
  if (isNew)
    m_nothingToUpload.clear();
}

size_t TextureManager::FindGlyphsGroup(strings::UniChar const & c) const
{
  auto const iter = lower_bound(m_glyphGroups.begin(), m_glyphGroups.end(), c, [](GlyphGroup const & g, strings::UniChar const & c)
  {
    return g.m_endChar < c;
  });

  if (iter == m_glyphGroups.end())
    return kInvalidGlyphGroup;

  return distance(m_glyphGroups.begin(), iter);
}

size_t TextureManager::FindGlyphsGroup(strings::UniString const & text) const
{
  size_t groupIndex = kInvalidGlyphGroup;
  for (auto const & c : text)
  {
    // skip glyphs which can be duplicated
    if (c < kDuplicatedGlyphsCount)
      continue;

    size_t currentIndex = FindGlyphsGroup(c);

    // an invalid glyph found
    if (currentIndex == kInvalidGlyphGroup)
    {
#if defined(TRACK_GLYPH_USAGE)
      GlyphUsageTracker::Instance().AddInvalidGlyph(text, c);
#endif
      return kInvalidGlyphGroup;
    }

    // check if each glyph in text id in one group
    if (groupIndex == kInvalidGlyphGroup)
      groupIndex = currentIndex;
    else if (groupIndex != currentIndex)
    {
#if defined(TRACK_GLYPH_USAGE)
      GlyphUsageTracker::Instance().AddUnexpectedGlyph(text, c, currentIndex, groupIndex);
#endif
      return kInvalidGlyphGroup;
    }
  }

  // all glyphs in duplicated range
  if (groupIndex == kInvalidGlyphGroup)
    groupIndex = FindGlyphsGroup(text[0]);

  return groupIndex;
}

size_t TextureManager::FindGlyphsGroup(TMultilineText const & text) const
{
  strings::UniString combinedString;
  MultilineTextToUniString(text, combinedString);

  return FindGlyphsGroup(combinedString);
}

uint32_t TextureManager::GetNumberOfUnfoundCharacters(strings::UniString const & text, int fixedHeight, HybridGlyphGroup const & group) const
{
  uint32_t cnt = 0;
  for (auto const & c : text)
    if (group.m_glyphs.find(make_pair(c, fixedHeight)) == group.m_glyphs.end())
      cnt++;

  return cnt;
}

void TextureManager::MarkCharactersUsage(strings::UniString const & text, int fixedHeight, HybridGlyphGroup & group)
{
  for (auto const & c : text)
    group.m_glyphs.emplace(make_pair(c, fixedHeight));
}

size_t TextureManager::FindHybridGlyphsGroup(strings::UniString const & text, int fixedHeight)
{
  if (m_hybridGlyphGroups.empty())
  {
    m_hybridGlyphGroups.push_back(HybridGlyphGroup());
    return 0;
  }

  HybridGlyphGroup & group = m_hybridGlyphGroups.back();
  bool hasEnoughSpace = true;
  if (group.m_texture != nullptr)
    hasEnoughSpace = group.m_texture->HasEnoughSpace(static_cast<uint32_t>(text.size()));

  // if we have got the only hybrid texture (in most cases it is) we can omit checking of glyphs usage
  if (hasEnoughSpace)
  {
    size_t const glyphsCount = group.m_glyphs.size() + text.size();
    if (m_hybridGlyphGroups.size() == 1 && glyphsCount < m_maxGlypsCount)
      return 0;
  }

  // looking for a hybrid texture which contains text entirely
  for (size_t i = 0; i < m_hybridGlyphGroups.size() - 1; i++)
    if (GetNumberOfUnfoundCharacters(text, fixedHeight, m_hybridGlyphGroups[i]) == 0)
      return i;

  // check if we can contain text in the last hybrid texture
  uint32_t const unfoundChars = GetNumberOfUnfoundCharacters(text, fixedHeight, group);
  uint32_t const newCharsCount = static_cast<uint32_t>(group.m_glyphs.size()) + unfoundChars;
  if (newCharsCount >= m_maxGlypsCount || !group.m_texture->HasEnoughSpace(unfoundChars))
    m_hybridGlyphGroups.push_back(HybridGlyphGroup());

  return m_hybridGlyphGroups.size() - 1;
}

size_t TextureManager::FindHybridGlyphsGroup(TMultilineText const & text, int fixedHeight)
{
  strings::UniString combinedString;
  MultilineTextToUniString(text, combinedString);

  return FindHybridGlyphsGroup(combinedString, fixedHeight);
}

void TextureManager::Init(Params const & params)
{
  m_textureAllocator = CreateAllocator();

  m_maxTextureSize = min(kMaxTextureSize, (uint32_t)GLFunctions::glGetInteger(gl_const::GLMaxTextureSize));

  GLFunctions::glPixelStore(gl_const::GLUnpackAlignment, 1);

  for (size_t i = 0; i < ARRAY_SIZE(kSymbolTextures); ++i)
  {
    m_symbolTextures.push_back(make_unique_dp<SymbolsTexture>(params.m_resPostfix, kSymbolTextures[i],
                                                              make_ref(m_textureAllocator)));
  }

  m_trafficArrowTexture = make_unique_dp<StaticTexture>("traffic-arrow", params.m_resPostfix,
                                                        make_ref(m_textureAllocator));

  // initialize patterns
  buffer_vector<buffer_vector<uint8_t, 8>, 64> patterns;
  double const visualScale = params.m_visualScale;
  ParsePatternsList(params.m_patterns, [&patterns, visualScale](buffer_vector<double, 8>  const & pattern)
  {
    buffer_vector<uint8_t, 8> p;
    for (size_t i = 0; i < pattern.size(); i++)
      p.push_back(pattern[i] * visualScale);
    patterns.push_back(move(p));
  });

  uint32_t stippleTextureHeight = max(my::NextPowOf2(static_cast<uint32_t>(patterns.size()) + kReservedPatterns),
                                      kMinStippleTextureHeight);
  stippleTextureHeight = min(m_maxTextureSize, stippleTextureHeight);
  m_stipplePenTexture = make_unique_dp<StipplePenTexture>(m2::PointU(kStippleTextureWidth, stippleTextureHeight),
                                                          make_ref(m_textureAllocator));
  LOG(LDEBUG, ("Patterns texture size = ", m_stipplePenTexture->GetWidth(), m_stipplePenTexture->GetHeight()));

  ref_ptr<StipplePenTexture> stipplePenTextureTex = make_ref(m_stipplePenTexture);
  for (auto it = patterns.begin(); it != patterns.end(); ++it)
    stipplePenTextureTex->ReservePattern(*it);

  // initialize colors
  buffer_vector<dp::Color, 256> colors;
  ParseColorsList(params.m_colors, [&colors](dp::Color const & color)
  {
    colors.push_back(color);
  });

  uint32_t colorTextureSize = max(my::NextPowOf2(floor(sqrt(colors.size() + kReservedColors))), kMinColorTextureSize);
  colorTextureSize *= ColorTexture::GetColorSizeInPixels();
  colorTextureSize = min(m_maxTextureSize, colorTextureSize);
  m_colorTexture = make_unique_dp<ColorTexture>(m2::PointU(colorTextureSize, colorTextureSize),
                                                make_ref(m_textureAllocator));
  LOG(LDEBUG, ("Colors texture size = ", m_colorTexture->GetWidth(), m_colorTexture->GetHeight()));

  ref_ptr<ColorTexture> colorTex = make_ref(m_colorTexture);
  for (auto it = colors.begin(); it != colors.end(); ++it)
    colorTex->ReserveColor(*it);

  // initialize glyphs
  m_glyphManager = make_unique_dp<GlyphManager>(params.m_glyphMngParams);

  uint32_t const textureSquare = m_maxTextureSize * m_maxTextureSize;
  uint32_t const baseGlyphHeight = params.m_glyphMngParams.m_baseGlyphHeight * kGlyphAreaMultiplier;
  uint32_t const avarageGlyphSquare = baseGlyphHeight * baseGlyphHeight;

  m_glyphGroups.push_back(GlyphGroup());
  m_maxGlypsCount = ceil(kGlyphAreaCoverage * textureSquare / avarageGlyphSquare);
  m_glyphManager->ForEachUnicodeBlock([this](strings::UniChar const & start, strings::UniChar const & end)
  {
    if (m_glyphGroups.empty())
    {
      m_glyphGroups.push_back(GlyphGroup(start, end));
      return;
    }

    GlyphGroup & group = m_glyphGroups.back();
    ASSERT_LESS_OR_EQUAL(group.m_endChar, start, ());

    if (end - group.m_startChar < m_maxGlypsCount)
      group.m_endChar = end;
    else
      m_glyphGroups.push_back(GlyphGroup(start, end));
  });
}

void TextureManager::Invalidate(string const & resPostfix)
{
  for (size_t i = 0; i < m_symbolTextures.size(); ++i)
  {
    ASSERT(m_symbolTextures[i] != nullptr, ());
    ref_ptr<SymbolsTexture> symbolsTexture = make_ref(m_symbolTextures[i]);
    symbolsTexture->Invalidate(resPostfix, make_ref(m_textureAllocator));
  }

  ASSERT(m_trafficArrowTexture != nullptr, ());
  ref_ptr<StaticTexture> staticTexture = make_ref(m_trafficArrowTexture);
  staticTexture->Invalidate(resPostfix, make_ref(m_textureAllocator));
}

void TextureManager::GetSymbolRegion(string const & symbolName, SymbolRegion & region)
{
  for (size_t i = 0; i < m_symbolTextures.size(); ++i)
  {
    ASSERT(m_symbolTextures[i] != nullptr, ());
    ref_ptr<SymbolsTexture> symbolsTexture = make_ref(m_symbolTextures[i]);
    if (symbolsTexture->IsSymbolContained(symbolName))
    {
      GetRegionBase(symbolsTexture, region, SymbolsTexture::SymbolKey(symbolName));
      return;
    }
  }
  LOG(LWARNING, ("Detected using of unknown symbol ", symbolName));
}

void TextureManager::GetStippleRegion(TStipplePattern const & pen, StippleRegion & region)
{
  GetRegionBase(make_ref(m_stipplePenTexture), region, StipplePenKey(pen));
}

void TextureManager::GetColorRegion(Color const & color, ColorRegion & region)
{
  GetRegionBase(make_ref(m_colorTexture), region, ColorKey(color));
}

void TextureManager::GetGlyphRegions(TMultilineText const & text, int fixedHeight, TMultilineGlyphsBuffer & buffers)
{
  CalcGlyphRegions<TMultilineText, TMultilineGlyphsBuffer>(text, fixedHeight, buffers);
}

void TextureManager::GetGlyphRegions(strings::UniString const & text, int fixedHeight, TGlyphsBuffer & regions)
{
  CalcGlyphRegions<strings::UniString, TGlyphsBuffer>(text, fixedHeight, regions);
}

uint32_t TextureManager::GetAbsentGlyphsCount(ref_ptr<Texture> texture, strings::UniString const & text, int fixedHeight)
{
  if (texture == nullptr)
    return 0;

  ASSERT(dynamic_cast<FontTexture *>(texture.get()) != nullptr, ());
  return static_cast<FontTexture *>(texture.get())->GetAbsentGlyphsCount(text, fixedHeight);
}

uint32_t TextureManager::GetAbsentGlyphsCount(ref_ptr<Texture> texture, TMultilineText const & text, int fixedHeight)
{
  if (texture == nullptr)
    return 0;

  uint32_t count = 0;
  for (size_t i = 0; i < text.size(); ++i)
    count += GetAbsentGlyphsCount(texture, text[i], fixedHeight);
  return count;
}

bool TextureManager::AreGlyphsReady(strings::UniString const & str, int fixedHeight) const
{
  return m_glyphManager->AreGlyphsReady(str, fixedHeight);
}

ref_ptr<Texture> TextureManager::GetSymbolsTexture() const
{
  ASSERT(!m_symbolTextures.empty(), ());
  return make_ref(m_symbolTextures[kDefaultSymbolsIndex]);
}

ref_ptr<Texture> TextureManager::GetTrafficArrowTexture() const
{
  return make_ref(m_trafficArrowTexture);
}

constexpr size_t TextureManager::GetInvalidGlyphGroup()
{
  return kInvalidGlyphGroup;
}

} // namespace dp
