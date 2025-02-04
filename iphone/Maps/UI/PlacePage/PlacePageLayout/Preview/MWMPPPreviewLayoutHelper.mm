#import "MWMPPPreviewLayoutHelper.h"
#import "MWMCommon.h"
#import "MWMDirectionView.h"
#import "MWMPlacePageData.h"
#import "MWMTableViewCell.h"
#import "Statistics.h"
#import "SwiftBridge.h"

#include "std/array.hpp"

#pragma mark - Base
// Base class for avoiding copy-paste in inheriting cells.
@interface _MWMPPPCellBase : MWMTableViewCell

@property(weak, nonatomic) IBOutlet UILabel * distance;
@property(weak, nonatomic) IBOutlet UIImageView * compass;
@property(weak, nonatomic) IBOutlet UIView * distanceView;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * trailing;
@property(copy, nonatomic) MWMVoidBlock tapOnDistance;

@end

@implementation _MWMPPPCellBase

- (void)layoutSubviews
{
  [super layoutSubviews];
  auto const inset = self.width / 2;
  self.separatorInset = {0, inset, 0, inset};
}

- (IBAction)tap
{
  if (self.tapOnDistance)
    self.tapOnDistance();
}

@end

#pragma mark - Title

@interface _MWMPPPTitle : _MWMPPPCellBase

@property(weak, nonatomic) IBOutlet UILabel * title;

@end

@implementation _MWMPPPTitle
@end

#pragma mark - External Title

@interface _MWMPPPExternalTitle : _MWMPPPCellBase

@property(weak, nonatomic) IBOutlet UILabel * externalTitle;

@end

@implementation _MWMPPPExternalTitle
@end

#pragma mark - Subtitle

@interface _MWMPPPSubtitle : _MWMPPPCellBase

@property(weak, nonatomic) IBOutlet UILabel * subtitle;

@end

@implementation _MWMPPPSubtitle
@end

#pragma mark - Schedule

@interface _MWMPPPSchedule : _MWMPPPCellBase

@property(weak, nonatomic) IBOutlet UILabel * schedule;

@end

@implementation _MWMPPPSchedule
@end

#pragma mark - Booking

@interface _MWMPPPBooking : MWMTableViewCell

@property(weak, nonatomic) IBOutlet UILabel * rating;
@property(weak, nonatomic) IBOutlet UILabel * pricing;

- (void)configWithRating:(NSString *)rating pricing:(NSString *)pricing;

@end

@implementation _MWMPPPBooking

- (void)configWithRating:(NSString *)rating pricing:(NSString *)pricing
{
  self.rating.text = rating;
  self.pricing.text = pricing;
}

@end

#pragma mark - Address

@interface _MWMPPPAddress : _MWMPPPCellBase

@property(weak, nonatomic) IBOutlet UILabel * address;

@end

@implementation _MWMPPPAddress
@end

@interface _MWMPPPSpace : _MWMPPPCellBase
@end

@implementation _MWMPPPSpace
@end

namespace
{
array<Class, 8> const kPreviewCells = {{[_MWMPPPTitle class], [_MWMPPPExternalTitle class],
                                        [_MWMPPPSubtitle class], [_MWMPPPSchedule class],
                                        [_MWMPPPBooking class], [_MWMPPPAddress class],
                                        [_MWMPPPSpace class], [MWMFBAdsBanner class]}};
}  // namespace

@interface MWMPPPreviewLayoutHelper ()

@property(weak, nonatomic) UITableView * tableView;

@property(weak, nonatomic) NSLayoutConstraint * distanceCellTrailing;
@property(weak, nonatomic) UIView * distanceView;

@property(weak, nonatomic) MWMPlacePageData * data;
@property(weak, nonatomic) id<MWMPPPreviewLayoutHelperDelegate> delegate;

@property(nonatomic) CGFloat leading;
@property(nonatomic) MWMDirectionView * directionView;
@property(copy, nonatomic) NSString * distance;
@property(weak, nonatomic) UIImageView * compass;
@property(nonatomic) NSIndexPath * lastCellIndexPath;
@property(nonatomic) BOOL lastCellIsBanner;
@property(nonatomic) NSUInteger distanceRow;

@property(weak, nonatomic) MWMFBAdsBanner * cachedBannerCell;

@end

@implementation MWMPPPreviewLayoutHelper

- (instancetype)initWithTableView:(UITableView *)tableView
{
  self = [super init];
  if (self)
  {
    _tableView = tableView;
    [self registerCells];
  }

  return self;
}

- (void)registerCells
{
  for (Class cls : kPreviewCells)
    [self.tableView registerWithCellClass:cls];
}

- (void)configWithData:(MWMPlacePageData *)data
{
  self.data = data;
  auto const & previewRows = data.previewRows;
  using place_page::PreviewRows;
  self.lastCellIsBanner = NO;
  self.lastCellIndexPath = [NSIndexPath indexPathForRow:previewRows.size() - 1 inSection:0];
  auto it = find(previewRows.begin(), previewRows.end(), PreviewRows::Space);
  if (it != previewRows.end())
    self.distanceRow = distance(previewRows.begin(), it) - 1;
}

- (UITableViewCell *)cellForRowAtIndexPath:(NSIndexPath *)indexPath withData:(MWMPlacePageData *)data
{
  using namespace place_page;
  auto tableView = self.tableView;
  auto const row = data.previewRows[indexPath.row];
  Class cls = kPreviewCells[static_cast<size_t>(row)];

  auto * c = [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath];
  switch(row)
  {
  case PreviewRows::Title:
    static_cast<_MWMPPPTitle *>(c).title.text = data.title;
    break;
  case PreviewRows::ExternalTitle:
    static_cast<_MWMPPPExternalTitle *>(c).externalTitle.text = data.externalTitle;
    break;
  case PreviewRows::Subtitle:
    static_cast<_MWMPPPSubtitle *>(c).subtitle.text = data.subtitle;
    break;
  case PreviewRows::Schedule:
  {
    auto scheduleCell = static_cast<_MWMPPPSchedule *>(c);
    switch (data.schedule)
    {
    case place_page::OpeningHours::AllDay:
      scheduleCell.schedule.text = L(@"twentyfour_seven");
      scheduleCell.schedule.textColor = [UIColor blackSecondaryText];
      break;
    case place_page::OpeningHours::Open:
      scheduleCell.schedule.text = L(@"editor_time_open");
      scheduleCell.schedule.textColor = [UIColor blackSecondaryText];
      break;
    case place_page::OpeningHours::Closed:
      scheduleCell.schedule.text = L(@"closed_now");
      scheduleCell.schedule.textColor = [UIColor red];
      break;
    case place_page::OpeningHours::Unknown: NSAssert(false, @"Incorrect schedule!"); break;
    }
    break;
  }
  case PreviewRows::Booking:
  {
    auto bookingCell = static_cast<_MWMPPPBooking *>(c);
    [bookingCell configWithRating:data.bookingRating pricing:data.bookingApproximatePricing];
    [data assignOnlinePriceToLabel:bookingCell.pricing];
    return bookingCell;
  }
  case PreviewRows::Address:
    static_cast<_MWMPPPAddress *>(c).address.text = data.address;
    break;
  case PreviewRows::Space:
    return c;
  case PreviewRows::Banner:
    auto bannerCell = static_cast<MWMFBAdsBanner *>(c);
    [bannerCell configWithAd:data.nativeAd];
    self.cachedBannerCell = bannerCell;
    return bannerCell;
  }

  auto baseCell = static_cast<_MWMPPPCellBase *>(c);

  if (indexPath.row == self.distanceRow)
    [self showDistanceOnCell:baseCell withData:data];
  else
    [self hideDistanceOnCell:baseCell];

  return c;
}

- (void)showDistanceOnCell:(_MWMPPPCellBase *)cell withData:(MWMPlacePageData *)data
{
  cell.trailing.priority = UILayoutPriorityDefaultLow;
  cell.distance.text = self.distance;
  auto directionView = self.directionView;
  cell.tapOnDistance = ^{
    [directionView show];
  };
  [cell.contentView setNeedsLayout];
  self.compass = cell.compass;
  self.distanceCellTrailing = cell.trailing;
  self.distanceView = cell.distanceView;
  cell.distanceView.hidden = NO;

  auto dv = self.directionView;
  dv.titleLabel.text = data.title;
  dv.typeLabel.text = data.subtitle;
  dv.distanceLabel.text = self.distance;
}

- (void)hideDistanceOnCell:(_MWMPPPCellBase *)cell
{
  cell.trailing.priority = UILayoutPriorityDefaultHigh;
  [cell.contentView setNeedsLayout];
  cell.distanceView.hidden = YES;
}

- (void)rotateDirectionArrowToAngle:(CGFloat)angle
{
  auto const t = CATransform3DMakeRotation(M_PI_2 - angle, 0, 0, 1);
  self.compass.layer.transform = t;
  self.directionView.directionArrow.layer.transform = t;
}

- (void)setDistanceToObject:(NSString *)distance
{
  if (!distance.length || [self.distance isEqualToString:distance])
    return;

  self.distance = distance;
  self.directionView.distanceLabel.text = distance;
}

- (void)insertRowAtTheEnd
{
  auto const & previewRows = self.data.previewRows;
  auto const size = previewRows.size();
  self.lastCellIsBanner = previewRows.back() == place_page::PreviewRows::Banner;
  self.lastCellIndexPath =
      [NSIndexPath indexPathForRow:size - 1
                         inSection:static_cast<NSUInteger>(place_page::Sections::Preview)];
  [self.tableView insertRowsAtIndexPaths:@[ self.lastCellIndexPath ]
                        withRowAnimation:UITableViewRowAnimationLeft];
  [self.delegate heightWasChanged];
}

- (CGFloat)height
{
  auto const rect = [self.tableView rectForRowAtIndexPath:self.lastCellIndexPath];
  auto const height = rect.origin.y + rect.size.height;
  if (!self.lastCellIndexPath)
    return height;

  auto constexpr gapBannerHeight = 4.0;
  CGFloat const excessHeight = self.cachedBannerCell.state == MWMFBAdsBannerStateDetailed
                                   ? [MWMFBAdsBanner detailedBannerExcessHeight]
                                   : 0;

  return height + gapBannerHeight - excessHeight;
}

- (void)layoutInOpenState:(BOOL)isOpen
{
  if (IPAD)
    return;

  [self.tableView update:^{
    self.cachedBannerCell.state = isOpen ? MWMFBAdsBannerStateDetailed : MWMFBAdsBannerStateCompact;
  }];
}

- (MWMDirectionView *)directionView
{
  if (!_directionView)
    _directionView = [[MWMDirectionView alloc] init];
  return _directionView;
}

@end
