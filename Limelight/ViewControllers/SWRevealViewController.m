/*

 Copyright (c) 2013 Joan Lluch <joan.lluch@sweetwilliamsl.com>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is furnished
 to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 Early code inspired on a similar class by Philip Kluz (Philip.Kluz@zuui.org)
 
*/

#import <QuartzCore/QuartzCore.h>

#import "SWRevealViewController.h"


#pragma mark - StatusBar Helper Function

// computes the required offset adjustment due to the status bar for the passed in view,
// it will return the statusBar height if view fully overlaps the statusBar, otherwise returns 0.0f
static CGFloat statusBarAdjustment( UIView* view )
{
    CGFloat adjustment = 0.0f;
    UIApplication *app = [UIApplication sharedApplication];
    CGRect viewFrame = [view convertRect:view.bounds toView:[app keyWindow]];
    CGRect statusBarFrame = [app statusBarFrame];
    
    if ( CGRectIntersectsRect(viewFrame, statusBarFrame) )
        adjustment = fminf(statusBarFrame.size.width, statusBarFrame.size.height);

    return adjustment;
}


#pragma mark - SWRevealView Class

@interface SWRevealView: UIView
{
    __weak SWRevealViewController *_c;
}

@property (nonatomic, readonly) UIView *rearView;
@property (nonatomic, readonly) UIView *rightView;
@property (nonatomic, readonly) UIView *frontView;
@property (nonatomic, assign) BOOL disableLayout;

@end


@interface SWRevealViewController()
- (void)_getRevealWidth:(CGFloat*)pRevealWidth revealOverDraw:(CGFloat*)pRevealOverdraw forSymetry:(int)symetry;
- (void)_getBounceBack:(BOOL*)pBounceBack pStableDrag:(BOOL*)pStableDrag forSymetry:(int)symetry;
- (void)_getAdjustedFrontViewPosition:(FrontViewPosition*)frontViewPosition forSymetry:(int)symetry;
@end


@implementation SWRevealView


static CGFloat scaledValue( CGFloat v1, CGFloat min2, CGFloat max2, CGFloat min1, CGFloat max1)
{
    CGFloat result = min2 + (v1-min1)*((max2-min2)/(max1-min1));
    if ( result != result ) return min2;  // nan
    if ( result < min2 ) return min2;
    if ( result > max2 ) return max2;
    return result;
}


- (id)initWithFrame:(CGRect)frame controller:(SWRevealViewController*)controller
{
    self = [super initWithFrame:frame];
    if ( self )
    {
        _c = controller;
        CGRect bounds = self.bounds;
    
        _frontView = [[UIView alloc] initWithFrame:bounds];
        _frontView.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
        [self reloadShadow];

        [self addSubview:_frontView];
    }
    return self;
}


- (void)reloadShadow
{
    CALayer *frontViewLayer = _frontView.layer;
    frontViewLayer.shadowColor = [_c.frontViewShadowColor CGColor];
    frontViewLayer.shadowOpacity = _c.frontViewShadowOpacity;
    frontViewLayer.shadowOffset = _c.frontViewShadowOffset;
    frontViewLayer.shadowRadius = _c.frontViewShadowRadius;
}


- (CGRect)hierarchycalFrameAdjustment:(CGRect)frame
{
    if ( _c.presentFrontViewHierarchically )
    {
        UINavigationBar *dummyBar = [[UINavigationBar alloc] init];
        CGFloat barHeight = [dummyBar sizeThatFits:CGSizeMake(100,100)].height;
        CGFloat offset = barHeight + statusBarAdjustment(self);
        frame.origin.y += offset;
        frame.size.height -= offset;
    }
    return frame;
}


- (void)prepareRearViewForPosition:(FrontViewPosition)newPosition
{
    if ( _rearView == nil )
    {
        _rearView = [[UIView alloc] initWithFrame:self.bounds];
        _rearView.autoresizingMask = /*UIViewAutoresizingFlexibleWidth|*/UIViewAutoresizingFlexibleHeight;
        [self insertSubview:_rearView belowSubview:_frontView];
    }
    
    CGFloat xLocation = [self frontLocationForPosition:_c.frontViewPosition];
    [self _layoutRearViewsForLocation:xLocation];
    [self _prepareForNewPosition:newPosition];
}


- (void)prepareRightViewForPosition:(FrontViewPosition)newPosition
{
    if ( _rightView == nil )
    {
        _rightView = [[UIView alloc] initWithFrame:self.bounds];
        _rightView.autoresizingMask = /*UIViewAutoresizingFlexibleWidth|*/UIViewAutoresizingFlexibleHeight;
        [self insertSubview:_rightView belowSubview:_frontView];
    }
    
    CGFloat xLocation = [self frontLocationForPosition:_c.frontViewPosition];
    [self _layoutRearViewsForLocation:xLocation];
    [self _prepareForNewPosition:newPosition];
}


- (CGFloat)frontLocationForPosition:(FrontViewPosition)frontViewPosition
{
    CGFloat revealWidth;
    CGFloat revealOverdraw;
    
    CGFloat location = 0.0f;
    
    int symetry = frontViewPosition<FrontViewPositionLeft? -1 : 1;
    [_c _getRevealWidth:&revealWidth revealOverDraw:&revealOverdraw forSymetry:symetry];
    [_c _getAdjustedFrontViewPosition:&frontViewPosition forSymetry:symetry];
    
    if ( frontViewPosition == FrontViewPositionRight )
        location = revealWidth;
    
    else if ( frontViewPosition > FrontViewPositionRight )
        location = revealWidth + revealOverdraw;

    return location*symetry;
}


- (void)dragFrontViewToXLocation:(CGFloat)xLocation
{
    CGRect bounds = self.bounds;
    
    xLocation = [self _adjustedDragLocationForLocation:xLocation];
    [self _layoutRearViewsForLocation:xLocation];
    
    CGRect frame = CGRectMake(xLocation, 0.0f, bounds.size.width, bounds.size.height);
    _frontView.frame = [self hierarchycalFrameAdjustment:frame];
}


# pragma mark - overrides

- (void)layoutSubviews
{
    if ( _disableLayout ) return;

    CGRect bounds = self.bounds;
    
    FrontViewPosition position = _c.frontViewPosition;
    CGFloat xLocation = [self frontLocationForPosition:position];
    
    // set rear view frames
    [self _layoutRearViewsForLocation:xLocation];
    
    // set front view frame
    CGRect frame = CGRectMake(xLocation, 0.0f, bounds.size.width, bounds.size.height);
    _frontView.frame = [self hierarchycalFrameAdjustment:frame];
    
    // setup front view shadow path if needed (front view loaded and not removed)
    UIViewController *frontViewController = _c.frontViewController;
    BOOL viewLoaded = frontViewController != nil && frontViewController.isViewLoaded;
    BOOL viewNotRemoved = position > FrontViewPositionLeftSideMostRemoved && position < FrontViewPositionRightMostRemoved;
    CGRect shadowBounds = viewLoaded && viewNotRemoved  ? _frontView.bounds : CGRectZero;
    
    UIBezierPath *shadowPath = [UIBezierPath bezierPathWithRect:shadowBounds];
    _frontView.layer.shadowPath = shadowPath.CGPath;
}


- (BOOL)pointInsideD:(CGPoint)point withEvent:(UIEvent *)event
{
    BOOL isInside = [super pointInside:point withEvent:event];
    if ( _c.extendsPointInsideHit )
    {
        if ( !isInside  && _rearView && [_c.rearViewController isViewLoaded] )
        {
            CGPoint pt = [self convertPoint:point toView:_rearView];
            isInside = [_rearView pointInside:pt withEvent:event];
        }
        
        if ( !isInside && _frontView && [_c.frontViewController isViewLoaded] )
        {
            CGPoint pt = [self convertPoint:point toView:_frontView];
            isInside = [_frontView pointInside:pt withEvent:event];
        }
        
        if ( !isInside && _rightView && [_c.rightViewController isViewLoaded] )
        {
            CGPoint pt = [self convertPoint:point toView:_rightView];
            isInside = [_rightView pointInside:pt withEvent:event];
        }
    }
    return isInside;
}


- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    BOOL isInside = [super pointInside:point withEvent:event];
    if ( !isInside && _c.extendsPointInsideHit )
    {
        UIView *testViews[] = { _rearView, _frontView, _rightView };
        UIViewController *testControllers[] = { _c.rearViewController, _c.frontViewController, _c.rightViewController };
        
        for ( NSInteger i=0 ; i<3 && !isInside ; i++ )
        {
            if ( testViews[i] && [testControllers[i] isViewLoaded] )
            {
                CGPoint pt = [self convertPoint:point toView:testViews[i]];
                isInside = [testViews[i] pointInside:pt withEvent:event];
            }
        }
    }
    return isInside;
}


# pragma mark - private


- (void)_layoutRearViewsForLocation:(CGFloat)xLocation
{
    CGRect bounds = self.bounds;
    
    CGFloat rearRevealWidth = _c.rearViewRevealWidth;
    if ( rearRevealWidth < 0) rearRevealWidth = bounds.size.width + _c.rearViewRevealWidth;
    
    CGFloat rearXLocation = scaledValue(xLocation, -_c.rearViewRevealDisplacement, 0, 0, rearRevealWidth);
    
    CGFloat rearWidth = rearRevealWidth + _c.rearViewRevealOverdraw;
    _rearView.frame = CGRectMake(rearXLocation, 0.0, rearWidth, bounds.size.height);
    
    CGFloat rightRevealWidth = _c.rightViewRevealWidth;
    if ( rightRevealWidth < 0) rightRevealWidth = bounds.size.width + _c.rightViewRevealWidth;
    
    CGFloat rightXLocation = scaledValue(xLocation, 0, _c.rightViewRevealDisplacement, -rightRevealWidth, 0);
    
    CGFloat rightWidth = rightRevealWidth + _c.rightViewRevealOverdraw;
    _rightView.frame = CGRectMake(bounds.size.width-rightWidth+rightXLocation, 0.0f, rightWidth, bounds.size.height);
}


- (void)_prepareForNewPosition:(FrontViewPosition)newPosition;
{
    if ( _rearView == nil || _rightView == nil )
        return;
    
    int symetry = newPosition<FrontViewPositionLeft? -1 : 1;

    NSArray *subViews = self.subviews;
    NSInteger rearIndex = [subViews indexOfObjectIdenticalTo:_rearView];
    NSInteger rightIndex = [subViews indexOfObjectIdenticalTo:_rightView];
    
    if ( (symetry < 0 && rightIndex < rearIndex) || (symetry > 0 && rearIndex < rightIndex) )
        [self exchangeSubviewAtIndex:rightIndex withSubviewAtIndex:rearIndex];
}


- (CGFloat)_adjustedDragLocationForLocation:(CGFloat)x
{
    CGFloat result;
    
    CGFloat revealWidth;
    CGFloat revealOverdraw;
    BOOL bounceBack;
    BOOL stableDrag;
    FrontViewPosition position = _c.frontViewPosition;
    
    int symetry = x<0 ? -1 : 1;
    
    [_c _getRevealWidth:&revealWidth revealOverDraw:&revealOverdraw forSymetry:symetry];
    [_c _getBounceBack:&bounceBack pStableDrag:&stableDrag forSymetry:symetry];
    
    BOOL stableTrack = !bounceBack || stableDrag || position==FrontViewPositionRightMost || position==FrontViewPositionLeftSideMost;
    if ( stableTrack )
    {
        revealWidth += revealOverdraw;
        revealOverdraw = 0.0f;
    }
    
    x = x * symetry;
    
    if (x <= revealWidth)
        result = x;         // Translate linearly.

    else if (x <= revealWidth+2*revealOverdraw)
        result = revealWidth + (x-revealWidth)/2;   // slow down translation by halph the movement.

    else
        result = revealWidth+revealOverdraw;        // keep at the rightMost location.
    
    return result * symetry;
}

@end


#pragma mark - SWContextTransitioningObject

@interface SWContextTransitionObject : NSObject<UIViewControllerContextTransitioning>
@end


@implementation SWContextTransitionObject
{
    __weak SWRevealViewController *_revealVC;
    UIView *_view;
    UIViewController *_toVC;
    UIViewController *_fromVC;
    void (^_completion)(void);
}

- (id)initWithRevealController:(SWRevealViewController*)revealVC containerView:(UIView*)view fromVC:(UIViewController*)fromVC
    toVC:(UIViewController*)toVC completion:(void (^)(void))completion
{
    self = [super init];
    if ( self )
    {
        _revealVC = revealVC;
        _view = view;
        _fromVC = fromVC;
        _toVC = toVC;
        _completion = completion;
    }
    return self;
}


- (UIView *)containerView
{
    return _view;
}


- (BOOL)isAnimated
{
    return YES;
}


- (BOOL)isInteractive
{
    return NO;  // not supported
}

- (BOOL)transitionWasCancelled
{
    return NO; // not supported
}

- (CGAffineTransform)targetTransform {
    return CGAffineTransformIdentity;
}

- (UIModalPresentationStyle)presentationStyle
{
    return UIModalPresentationNone;  // not applicable
}


- (void)updateInteractiveTransition:(CGFloat)percentComplete
{
    // not supported
}


- (void)finishInteractiveTransition
{
    // not supported
}


- (void)cancelInteractiveTransition
{
    // not supported
}


- (void)completeTransition:(BOOL)didComplete
{
    _completion();
}


- (UIViewController *)viewControllerForKey:(NSString *)key
{
    if ( [key isEqualToString:UITransitionContextFromViewControllerKey] )
        return _fromVC;
    
    if ( [key isEqualToString:UITransitionContextToViewControllerKey] )
        return _toVC;
    
    return nil;
}


- (UIView *)viewForKey:(NSString *)key
{
    return nil;
}


- (CGRect)initialFrameForViewController:(UIViewController *)vc
{
    return _view.bounds;
}


- (CGRect)finalFrameForViewController:(UIViewController *)vc
{
    return _view.bounds;
}

@end


#pragma mark - SWDefaultAnimationController Class

@interface SWDefaultAnimationController : NSObject<UIViewControllerAnimatedTransitioning>
@end

@implementation SWDefaultAnimationController
{
    NSTimeInterval _duration;
}


- (id)initWithDuration:(NSTimeInterval)duration
{
    self = [super init];
    if ( self )
    {
        _duration = duration;
    }
    return self;
}


- (NSTimeInterval)transitionDuration:(id<UIViewControllerContextTransitioning>)transitionContext
{
    return _duration;
}


- (void)animateTransition:(id <UIViewControllerContextTransitioning>)transitionContext
{
    UIViewController *fromViewController = [transitionContext viewControllerForKey:UITransitionContextFromViewControllerKey];
    UIViewController *toViewController = [transitionContext viewControllerForKey:UITransitionContextToViewControllerKey];

    if ( fromViewController )
    {
        [UIView transitionFromView:fromViewController.view toView:toViewController.view duration:_duration
            options:UIViewAnimationOptionTransitionCrossDissolve|UIViewAnimationOptionOverrideInheritedOptions
            completion:^(BOOL finished) { [transitionContext completeTransition:finished]; }];
    }
    else
    {
        // tansitionFromView does not correctly handle the case where the fromView is nil (at least on iOS7) it just pops up the toView view with no animation,
        // so in such case we replace the crossDissolve animation by a simple alpha animation on the appearing view
        UIView *toView = toViewController.view;
        CGFloat alpha = toView.alpha;
        toView.alpha = 0;
        
        [UIView animateWithDuration:_duration delay:0 options:UIViewAnimationOptionCurveEaseOut
        animations:^{ toView.alpha = alpha;}
        completion:^(BOOL finished) { [transitionContext completeTransition:finished];}];
    }
}

@end


#pragma mark - SWRevealViewControllerPanGestureRecognizer

#import <UIKit/UIGestureRecognizerSubclass.h>

@interface SWRevealViewControllerPanGestureRecognizer : UIPanGestureRecognizer
@end

@implementation SWRevealViewControllerPanGestureRecognizer
{
    BOOL _dragging;
    CGPoint _beginPoint;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    [super touchesBegan:touches withEvent:event];
   
    UITouch *touch = [touches anyObject];
    _beginPoint = [touch locationInView:self.view];
    _dragging = NO;
}


- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    [super touchesMoved:touches withEvent:event];
    
    if ( _dragging || self.state == UIGestureRecognizerStateFailed)
        return;
    
    const int kDirectionPanThreshold = 5;
    
    UITouch *touch = [touches anyObject];
    CGPoint nowPoint = [touch locationInView:self.view];
    
    if (fabs(nowPoint.x - _beginPoint.x) > kDirectionPanThreshold) _dragging = YES;
    else if (fabs(nowPoint.y - _beginPoint.y) > kDirectionPanThreshold) self.state = UIGestureRecognizerStateFailed;
}

@end


#pragma mark - SWRevealViewController Class

@interface SWRevealViewController()<UIGestureRecognizerDelegate>
{
    SWRevealView *_contentView;
    UIPanGestureRecognizer *_panGestureRecognizer;
    UITapGestureRecognizer *_tapGestureRecognizer;
    FrontViewPosition _frontViewPosition;
    FrontViewPosition _rearViewPosition;
    FrontViewPosition _rightViewPosition;
    SWContextTransitionObject *_rearTransitioningController;
    SWContextTransitionObject *_frontTransitioningController;
    SWContextTransitionObject *_rightTransitioningController;
}
@end


@implementation SWRevealViewController
{
    FrontViewPosition _panInitialFrontPosition;
    NSMutableArray *_animationQueue;
    BOOL _userInteractionStore;
    UIViewController *_primaryViewController;
}

const int FrontViewPositionNone = 0xff;


#pragma mark - Init

- (id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if ( self )
    {
        [self _initDefaultProperties];
    }    
    return self;
}


- (id)init
{
    return [self initWithRearViewController:nil frontViewController:nil];
}


- (id)initWithRearViewController:(UIViewController *)rearViewController frontViewController:(UIViewController *)frontViewController;
{
    self = [super init];
    if ( self )
    {
        [self _initDefaultProperties];
        [self _performTransitionOperation:SWRevealControllerOperationReplaceRearController withViewController:rearViewController animated:NO];
        [self _performTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:frontViewController animated:NO];
    }
    return self;
}


- (void)_initDefaultProperties
{
    _frontViewPosition = FrontViewPositionLeft;
    _rearViewPosition = FrontViewPositionLeft;
    _rightViewPosition = FrontViewPositionLeft;
    _rearViewRevealWidth = 260.0f;
    _rearViewRevealOverdraw = 60.0f;
    _rearViewRevealDisplacement = 40.0f;
    _rightViewRevealWidth = 260.0f;
    _rightViewRevealOverdraw = 60.0f;
    _rightViewRevealDisplacement = 40.0f;
    _bounceBackOnOverdraw = YES;
    _bounceBackOnLeftOverdraw = YES;
    _stableDragOnOverdraw = NO;
    _stableDragOnLeftOverdraw = NO;
    _presentFrontViewHierarchically = NO;
    _quickFlickVelocity = 250.0f;
    _toggleAnimationDuration = 0.3;
    _toggleAnimationType = SWRevealToggleAnimationTypeSpring;
    _springDampingRatio = 1;
    _replaceViewAnimationDuration = 0.25;
    _frontViewShadowRadius = 2.5f;
    _frontViewShadowOffset = CGSizeMake(0.0f, 2.5f);
    _frontViewShadowOpacity = 1.0f;
    _frontViewShadowColor = [UIColor blackColor];
    _userInteractionStore = YES;
    _animationQueue = [NSMutableArray array];
    _draggableBorderWidth = 0.0f;
    _clipsViewsToBounds = NO;
    _extendsPointInsideHit = NO;
}


#pragma mark - StatusBar

- (UIViewController *)childViewControllerForStatusBarStyle
{
    int positionDif =  _frontViewPosition - FrontViewPositionLeft;
    
    UIViewController *controller = _frontViewController;
    if ( positionDif > 0 ) controller = _rearViewController;
    else if ( positionDif < 0 ) controller = _rightViewController;
    
    return controller;
}

- (UIViewController *)childViewControllerForStatusBarHidden
{
    UIViewController *controller = [self childViewControllerForStatusBarStyle];
    return controller;
}


#pragma mark - View lifecycle

- (void)loadView
{
    // Do not call super, to prevent the apis from unfruitful looking for inexistent xibs!
    //[super loadView];
    
    // load any defined front/rear controllers from the storyboard before
    [self loadStoryboardControllers];
    
    // This is what Apple used to tell us to set as the initial frame, which is of course totally irrelevant
    // with view controller containment patterns, let's leave it for the sake of it!
    // CGRect frame = [[UIScreen mainScreen] applicationFrame];
    
    // On iOS7 the applicationFrame does not return the whole screen. This is possibly a bug.
    // As a workaround we use the screen bounds, this still works on iOS6, any zero based frame would work anyway!
    CGRect frame = [[UIScreen mainScreen] bounds];

    // create a custom content view for the controller
    _contentView = [[SWRevealView alloc] initWithFrame:frame controller:self];
    
    // set the content view to resize along with its superview
    [_contentView setAutoresizingMask:UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight];
    
    // set the content view to clip its bounds if requested
    [_contentView setClipsToBounds:_clipsViewsToBounds];

    // set our contentView to the controllers view
    self.view = _contentView;
    
    // Apple also tells us to do this:
    _contentView.backgroundColor = [UIColor blackColor];
    
    // we set the current frontViewPosition to none before seting the
    // desired initial position, this will force proper controller reload
    FrontViewPosition initialPosition = _frontViewPosition;
    _frontViewPosition = FrontViewPositionNone;
    _rearViewPosition = FrontViewPositionNone;
    _rightViewPosition = FrontViewPositionNone;
    
    // now set the desired initial position
    [self _setFrontViewPosition:initialPosition withDuration:0.0];
}


- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];

    // Uncomment the following code if you want the child controllers
    // to be loaded at this point.
    //
    // We leave this commented out because we think loading childs here is conceptually wrong.
    // Instead, we refrain view loads until necesary, for example we may never load
    // the rear controller view -or the front controller view- if it is never displayed.
    //
    // If you need to manipulate views of any of your child controllers in an override
    // of this method, you can load yourself the views explicitly on your overriden method.
    // However we discourage it as an app following the MVC principles should never need to do so
        
//  [_frontViewController view];
//  [_rearViewController view];

    // we store at this point the view's user interaction state as we may temporarily disable it
    // and resume it back to the previous state, it is possible to override this behaviour by
    // intercepting it on the panGestureBegan and panGestureEnded delegates
    _userInteractionStore = _contentView.userInteractionEnabled;
}

#pragma mark - Public methods and property accessors

- (void)setFrontViewController:(UIViewController *)frontViewController
{
    [self setFrontViewController:frontViewController animated:NO];
}


- (void)setFrontViewController:(UIViewController *)frontViewController animated:(BOOL)animated
{
    if ( ![self isViewLoaded])
    {
        [self _performTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:frontViewController animated:NO];
        return;
    }
    
    [self _dispatchTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:frontViewController animated:animated];
}


- (void)pushFrontViewController:(UIViewController *)frontViewController animated:(BOOL)animated
{
    if ( ![self isViewLoaded])
    {
        [self _performTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:frontViewController animated:NO];
        return;
    }
    
    [self _dispatchPushFrontViewController:frontViewController animated:animated];
}


- (void)setRearViewController:(UIViewController *)rearViewController
{
    [self setRearViewController:rearViewController animated:NO];
}


- (void)setRearViewController:(UIViewController *)rearViewController animated:(BOOL)animated
{
    if ( ![self isViewLoaded])
    {
        [self _performTransitionOperation:SWRevealControllerOperationReplaceRearController withViewController:rearViewController animated:NO];
        return;
    }

    [self _dispatchTransitionOperation:SWRevealControllerOperationReplaceRearController withViewController:rearViewController animated:animated];
}


- (void)setRightViewController:(UIViewController *)rightViewController
{
    [self setRightViewController:rightViewController animated:NO];
}


- (void)setRightViewController:(UIViewController *)rightViewController animated:(BOOL)animated
{
    if ( ![self isViewLoaded])
    {
        [self _performTransitionOperation:SWRevealControllerOperationReplaceRightController withViewController:rightViewController animated:NO];
        return;
    }

    [self _dispatchTransitionOperation:SWRevealControllerOperationReplaceRightController withViewController:rightViewController animated:animated];
}


- (void)revealToggleAnimated:(BOOL)animated
{
    FrontViewPosition toggledFrontViewPosition = FrontViewPositionLeft;
    if (_frontViewPosition <= FrontViewPositionLeft)
        toggledFrontViewPosition = FrontViewPositionRight;
    
    [self setFrontViewPosition:toggledFrontViewPosition animated:animated];
}


- (void)rightRevealToggleAnimated:(BOOL)animated
{
    FrontViewPosition toggledFrontViewPosition = FrontViewPositionLeft;
    if (_frontViewPosition >= FrontViewPositionLeft)
        toggledFrontViewPosition = FrontViewPositionLeftSide;
    
    [self setFrontViewPosition:toggledFrontViewPosition animated:animated];
}


- (void)setFrontViewPosition:(FrontViewPosition)frontViewPosition
{
    [self setFrontViewPosition:frontViewPosition animated:NO];
}


- (void)setFrontViewPosition:(FrontViewPosition)frontViewPosition animated:(BOOL)animated
{
    if ( ![self isViewLoaded] )
    {
        _frontViewPosition = frontViewPosition;
        _rearViewPosition = frontViewPosition;
        _rightViewPosition = frontViewPosition;
        return;
    }
    
    [self _dispatchSetFrontViewPosition:frontViewPosition animated:animated];
}


- (void)setFrontViewShadowRadius:(CGFloat)frontViewShadowRadius
{
    _frontViewShadowRadius = frontViewShadowRadius;
    [_contentView reloadShadow];
}


- (void)setFrontViewShadowOffset:(CGSize)frontViewShadowOffset
{
    _frontViewShadowOffset = frontViewShadowOffset;
    [_contentView reloadShadow];
}


- (void)setFrontViewShadowOpacity:(CGFloat)frontViewShadowOpacity
{
    _frontViewShadowOpacity = frontViewShadowOpacity;
    [_contentView reloadShadow];
}


- (void)setFrontViewShadowColor:(UIColor *)frontViewShadowColor
{
    _frontViewShadowColor = frontViewShadowColor;
    [_contentView reloadShadow];
}


- (UIPanGestureRecognizer*)panGestureRecognizer
{
    if ( _panGestureRecognizer == nil )
    {
        _panGestureRecognizer = [[SWRevealViewControllerPanGestureRecognizer alloc] initWithTarget:self action:@selector(_handleRevealGesture:)];
        _panGestureRecognizer.delegate = self;
        [_contentView.frontView addGestureRecognizer:_panGestureRecognizer];
    }
    return _panGestureRecognizer;
}


- (UITapGestureRecognizer*)tapGestureRecognizer
{
    if ( _tapGestureRecognizer == nil )
    {
        UITapGestureRecognizer *tapRecognizer =
            [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_handleTapGesture:)];
        
        tapRecognizer.delegate = self;
        [_contentView.frontView addGestureRecognizer:tapRecognizer];
        _tapGestureRecognizer = tapRecognizer ;
    }
    return _tapGestureRecognizer;
}


- (void)setClipsViewsToBounds:(BOOL)clipsViewsToBounds
{
    _clipsViewsToBounds = clipsViewsToBounds;
    [_contentView setClipsToBounds:clipsViewsToBounds];
}



#pragma mark - Provided acction methods

- (IBAction)revealToggle:(id)sender
{    
    [self revealToggleAnimated:YES];
}


- (IBAction)rightRevealToggle:(id)sender
{    
    [self rightRevealToggleAnimated:YES];
}


#pragma mark - UserInteractionEnabling

// disable userInteraction on the entire control
- (void)_disableUserInteraction
{
    [_contentView setUserInteractionEnabled:NO];
    [_contentView setDisableLayout:YES];
}

// restore userInteraction on the control
- (void)_restoreUserInteraction
{
    // we use the stored userInteraction state just in case a developer decided
    // to have our view interaction disabled beforehand
    [_contentView setUserInteractionEnabled:_userInteractionStore];
    [_contentView setDisableLayout:NO];
}


#pragma mark - PanGesture progress notification

- (void)_notifyPanGestureBegan
{
    if ( [_delegate respondsToSelector:@selector(revealControllerPanGestureBegan:)] )
        [_delegate revealControllerPanGestureBegan:self];
    
    CGFloat xLocation, dragProgress, overProgress;
    [self _getDragLocation:&xLocation progress:&dragProgress overdrawProgress:&overProgress];
    
    if ( [_delegate respondsToSelector:@selector(revealController:panGestureBeganFromLocation:progress:overProgress:)] )
        [_delegate revealController:self panGestureBeganFromLocation:xLocation progress:dragProgress overProgress:overProgress];
    
    else if ( [_delegate respondsToSelector:@selector(revealController:panGestureBeganFromLocation:progress:)] )
        [_delegate revealController:self panGestureBeganFromLocation:xLocation progress:dragProgress];
}

- (void)_notifyPanGestureMoved
{
    CGFloat xLocation, dragProgress, overProgress;
    [self _getDragLocation:&xLocation progress:&dragProgress overdrawProgress:&overProgress];
    
    if ( [_delegate respondsToSelector:@selector(revealController:panGestureMovedToLocation:progress:overProgress:)] )
        [_delegate revealController:self panGestureMovedToLocation:xLocation progress:dragProgress overProgress:overProgress];
    
    else if ( [_delegate respondsToSelector:@selector(revealController:panGestureMovedToLocation:progress:)] )
        [_delegate revealController:self panGestureMovedToLocation:xLocation progress:dragProgress];
}

- (void)_notifyPanGestureEnded
{
    CGFloat xLocation, dragProgress, overProgress;
    [self _getDragLocation:&xLocation progress:&dragProgress overdrawProgress:&overProgress];
    
    if ( [_delegate respondsToSelector:@selector(revealController:panGestureEndedToLocation:progress:overProgress:)] )
        [_delegate revealController:self panGestureEndedToLocation:xLocation progress:dragProgress overProgress:overProgress];
    
    else if ( [_delegate respondsToSelector:@selector(revealController:panGestureEndedToLocation:progress:)] )
        [_delegate revealController:self panGestureEndedToLocation:xLocation progress:dragProgress];
    
    if ( [_delegate respondsToSelector:@selector(revealControllerPanGestureEnded:)] )
        [_delegate revealControllerPanGestureEnded:self];
}


#pragma mark - Symetry

- (void)_getRevealWidth:(CGFloat*)pRevealWidth revealOverDraw:(CGFloat*)pRevealOverdraw forSymetry:(int)symetry
{
    if ( symetry < 0 ) *pRevealWidth = _rightViewRevealWidth, *pRevealOverdraw = _rightViewRevealOverdraw;
    else *pRevealWidth = _rearViewRevealWidth, *pRevealOverdraw = _rearViewRevealOverdraw;
    
    if (*pRevealWidth < 0) *pRevealWidth = _contentView.bounds.size.width + *pRevealWidth;
}

- (void)_getBounceBack:(BOOL*)pBounceBack pStableDrag:(BOOL*)pStableDrag forSymetry:(int)symetry
{
    if ( symetry < 0 ) *pBounceBack = _bounceBackOnLeftOverdraw, *pStableDrag = _stableDragOnLeftOverdraw;
    else *pBounceBack = _bounceBackOnOverdraw, *pStableDrag = _stableDragOnOverdraw;
}

- (void)_getAdjustedFrontViewPosition:(FrontViewPosition*)frontViewPosition forSymetry:(int)symetry
{
    if ( symetry < 0 ) *frontViewPosition = FrontViewPositionLeft + symetry*(*frontViewPosition-FrontViewPositionLeft);
}

- (void)_getDragLocationx:(CGFloat*)xLocation progress:(CGFloat*)progress
{
    UIView *frontView = _contentView.frontView;
    *xLocation = frontView.frame.origin.x;

    int symetry = *xLocation<0 ? -1 : 1;
    
    CGFloat xWidth = symetry < 0 ? _rightViewRevealWidth : _rearViewRevealWidth;
    if ( xWidth < 0 ) xWidth = _contentView.bounds.size.width + xWidth;
    
    *progress = *xLocation/xWidth * symetry;
}

- (void)_getDragLocation:(CGFloat*)xLocation progress:(CGFloat*)progress overdrawProgress:(CGFloat*)overProgress
{
    UIView *frontView = _contentView.frontView;
    *xLocation = frontView.frame.origin.x;

    int symetry = *xLocation<0 ? -1 : 1;
    
    CGFloat xWidth = symetry < 0 ? _rightViewRevealWidth : _rearViewRevealWidth;
    CGFloat xOverWidth = symetry < 0 ? _rightViewRevealOverdraw : _rearViewRevealOverdraw;
    
    if ( xWidth < 0 ) xWidth = _contentView.bounds.size.width + xWidth;
    
    *progress = *xLocation*symetry/xWidth;
    *overProgress = (*xLocation*symetry-xWidth)/xOverWidth;
}


#pragma mark - Deferred block execution queue

// Define a convenience macro to enqueue single statements
#define _enqueue(code) [self _enqueueBlock:^{code;}];

// Defers the execution of the passed in block until a paired _dequeue call is received,
// or executes the block right away if no pending requests are present.
- (void)_enqueueBlock:(void (^)(void))block
{
    [_animationQueue insertObject:block atIndex:0];
    if ( _animationQueue.count == 1)
    {
        block();
    }
}

// Removes the top most block in the queue and executes the following one if any.
// Calls to this method must be paired with calls to _enqueueBlock, particularly it may be called
// from within a block passed to _enqueueBlock to remove itself when done with animations.  
- (void)_dequeue
{
    [_animationQueue removeLastObject];

    if ( _animationQueue.count > 0 )
    {
        void (^block)(void) = [_animationQueue lastObject];
        block();
    }
}


#pragma mark - Gesture Delegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)recognizer
{
    // only allow gesture if no previous request is in process
    if ( _animationQueue.count == 0 )
    {
        if ( recognizer == _panGestureRecognizer )
            return [self _panGestureShouldBegin];
        
        if ( recognizer == _tapGestureRecognizer )
            return [self _tapGestureShouldBegin];
    }

    return NO;
}


- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ( gestureRecognizer == _panGestureRecognizer )
    {
        if ( [_delegate respondsToSelector:@selector(revealController:panGestureRecognizerShouldRecognizeSimultaneouslyWithGestureRecognizer:)] )
            if ( [_delegate revealController:self panGestureRecognizerShouldRecognizeSimultaneouslyWithGestureRecognizer:otherGestureRecognizer] != NO )
                return YES;
    }
    if ( gestureRecognizer == _tapGestureRecognizer )
    {
        if ( [_delegate respondsToSelector:@selector(revealController:tapGestureRecognizerShouldRecognizeSimultaneouslyWithGestureRecognizer:)] )
            if ( [_delegate revealController:self tapGestureRecognizerShouldRecognizeSimultaneouslyWithGestureRecognizer:otherGestureRecognizer] != NO )
                return YES;
    }
    
    return NO;
}


- (BOOL)_tapGestureShouldBegin
{
    if ( _frontViewPosition == FrontViewPositionLeft ||
        _frontViewPosition == FrontViewPositionRightMostRemoved ||
        _frontViewPosition == FrontViewPositionLeftSideMostRemoved )
            return NO;
    
    // forbid gesture if the following delegate is implemented and returns NO
    if ( [_delegate respondsToSelector:@selector(revealControllerTapGestureShouldBegin:)] )
        if ( [_delegate revealControllerTapGestureShouldBegin:self] == NO )
            return NO;
    
    return YES;
}


- (BOOL)_panGestureShouldBegin
{
    // forbid gesture if the initial translation is not horizontal
    UIView *recognizerView = _panGestureRecognizer.view;
    CGPoint translation = [_panGestureRecognizer translationInView:recognizerView];
//        Log(LOG_D, @"translation:%@", NSStringFromCGPoint(translation) );
//    if ( fabs(translation.y/translation.x) > 1 )
//        return NO;

    // forbid gesture if the following delegate is implemented and returns NO
    if ( [_delegate respondsToSelector:@selector(revealControllerPanGestureShouldBegin:)] )
        if ( [_delegate revealControllerPanGestureShouldBegin:self] == NO )
            return NO;

    CGFloat xLocation = [_panGestureRecognizer locationInView:recognizerView].x;
    CGFloat width = recognizerView.bounds.size.width;
    
    BOOL draggableBorderAllowing = (
         /*_frontViewPosition != FrontViewPositionLeft ||*/ _draggableBorderWidth == 0.0f ||
         (_rearViewController && xLocation <= _draggableBorderWidth) ||
         (_rightViewController && xLocation >= (width - _draggableBorderWidth)) );
    
    
    BOOL translationForbidding = ( _frontViewPosition == FrontViewPositionLeft &&
        ((_rearViewController == nil && translation.x > 0) || (_rightViewController == nil && translation.x < 0)) );

    // allow gesture only within the bounds defined by the draggableBorderWidth property
    return draggableBorderAllowing && !translationForbidding ;
}


#pragma mark - Gesture Based Reveal

- (void)_handleTapGesture:(UITapGestureRecognizer *)recognizer
{
    NSTimeInterval duration = _toggleAnimationDuration;
    [self _setFrontViewPosition:FrontViewPositionLeft withDuration:duration];
}


- (void)_handleRevealGesture:(UIPanGestureRecognizer *)recognizer
{
    switch ( recognizer.state )
    {
        case UIGestureRecognizerStateBegan:
            [self _handleRevealGestureStateBeganWithRecognizer:recognizer];
            break;
            
        case UIGestureRecognizerStateChanged:
            [self _handleRevealGestureStateChangedWithRecognizer:recognizer];
            break;
            
        case UIGestureRecognizerStateEnded:
            [self _handleRevealGestureStateEndedWithRecognizer:recognizer];
            break;
            
        case UIGestureRecognizerStateCancelled:
        //case UIGestureRecognizerStateFailed:
            [self _handleRevealGestureStateCancelledWithRecognizer:recognizer];
            break;
            
        default:
            break;
    }
}


- (void)_handleRevealGestureStateBeganWithRecognizer:(UIPanGestureRecognizer *)recognizer
{
    // we know that we will not get here unless the animationQueue is empty because the recognizer
    // delegate prevents it, however we do not want any forthcoming programatic actions to disturb
    // the gesture, so we just enqueue a dummy block to ensure any programatic acctions will be
    // scheduled after the gesture is completed
    [self _enqueueBlock:^{}]; // <-- dummy block

    // we store the initial position and initialize a target position
    _panInitialFrontPosition = _frontViewPosition;

    // we disable user interactions on the views, however programatic accions will still be
    // enqueued to be performed after the gesture completes
    [self _disableUserInteraction];
    [self _notifyPanGestureBegan];
}


- (void)_handleRevealGestureStateChangedWithRecognizer:(UIPanGestureRecognizer *)recognizer
{
    CGFloat translation = [recognizer translationInView:_contentView].x;
    
    CGFloat baseLocation = [_contentView frontLocationForPosition:_panInitialFrontPosition];
    CGFloat xLocation = baseLocation + translation;
    
    if ( xLocation < 0 )
    {
        if ( _rightViewController == nil ) xLocation = 0;
        [self _rightViewDeploymentForNewFrontViewPosition:FrontViewPositionLeftSide]();
        [self _rearViewDeploymentForNewFrontViewPosition:FrontViewPositionLeftSide]();
    }
    
    if ( xLocation > 0 )
    {
        if ( _rearViewController == nil ) xLocation = 0;
        [self _rightViewDeploymentForNewFrontViewPosition:FrontViewPositionRight]();
        [self _rearViewDeploymentForNewFrontViewPosition:FrontViewPositionRight]();
    }
    
    [_contentView dragFrontViewToXLocation:xLocation];
    [self _notifyPanGestureMoved];
}


- (void)_handleRevealGestureStateEndedWithRecognizer:(UIPanGestureRecognizer *)recognizer
{
    UIView *frontView = _contentView.frontView;
    
    CGFloat xLocation = frontView.frame.origin.x;
    CGFloat velocity = [recognizer velocityInView:_contentView].x;
    //Log(LOG_D, @"Velocity:%1.4f", velocity);
    
    // depending on position we compute a simetric replacement of widths and positions
    int symetry = xLocation<0 ? -1 : 1;
    
    // simetring computing of widths
    CGFloat revealWidth ;
    CGFloat revealOverdraw ;
    BOOL bounceBack;
    BOOL stableDrag;
    
    [self _getRevealWidth:&revealWidth revealOverDraw:&revealOverdraw forSymetry:symetry];
    [self _getBounceBack:&bounceBack pStableDrag:&stableDrag forSymetry:symetry];
    
    // simetric replacement of position
    xLocation = xLocation * symetry;
    
    // initially we assume drag to left and default duration
    FrontViewPosition frontViewPosition = FrontViewPositionLeft;
    NSTimeInterval duration = _toggleAnimationDuration;

    // Velocity driven change:
    if (fabs(velocity) > _quickFlickVelocity)
    {
        // we may need to set the drag position and to adjust the animation duration
        CGFloat journey = xLocation;
        if (velocity*symetry > 0.0f)
        {
            frontViewPosition = FrontViewPositionRight;
            journey = revealWidth - xLocation;
            if (xLocation > revealWidth)
            {
                if (!bounceBack && stableDrag /*&& xPosition > _rearViewRevealWidth+_rearViewRevealOverdraw*0.5f*/)
                {
                    frontViewPosition = FrontViewPositionRightMost;
                    journey = revealWidth+revealOverdraw - xLocation;
                }
            }
        }
        
        duration = fabs(journey/velocity);
    }
    
    // Position driven change:
    else
    {    
        // we may need to set the drag position        
        if (xLocation > revealWidth*0.5f)
        {
            frontViewPosition = FrontViewPositionRight;
            if (xLocation > revealWidth)
            {
                if (bounceBack)
                    frontViewPosition = FrontViewPositionLeft;

                else if (stableDrag && xLocation > revealWidth+revealOverdraw*0.5f)
                    frontViewPosition = FrontViewPositionRightMost;
            }
        }
    }
    
    // symetric replacement of frontViewPosition
    [self _getAdjustedFrontViewPosition:&frontViewPosition forSymetry:symetry];
    
    // restore user interaction and animate to the final position
    [self _restoreUserInteraction];
    [self _notifyPanGestureEnded];
    [self _setFrontViewPosition:frontViewPosition withDuration:duration];
}


- (void)_handleRevealGestureStateCancelledWithRecognizer:(UIPanGestureRecognizer *)recognizer
{    
    [self _restoreUserInteraction];
    [self _notifyPanGestureEnded];
    [self _dequeue];
}


#pragma mark Enqueued position and controller setup

- (void)_dispatchSetFrontViewPosition:(FrontViewPosition)frontViewPosition animated:(BOOL)animated
{
    NSTimeInterval duration = animated?_toggleAnimationDuration:0.0;
    __weak SWRevealViewController *theSelf = self;
    _enqueue( [theSelf _setFrontViewPosition:frontViewPosition withDuration:duration] );
}


- (void)_dispatchPushFrontViewController:(UIViewController *)newFrontViewController animated:(BOOL)animated
{
    FrontViewPosition preReplacementPosition = FrontViewPositionLeft;
    if ( _frontViewPosition > FrontViewPositionLeft ) preReplacementPosition = FrontViewPositionRightMost;
    if ( _frontViewPosition < FrontViewPositionLeft ) preReplacementPosition = FrontViewPositionLeftSideMost;
    
    NSTimeInterval duration = animated?_toggleAnimationDuration:0.0;
    NSTimeInterval firstDuration = duration;
    int initialPosDif = (int) labs( _frontViewPosition - preReplacementPosition );
    if ( initialPosDif == 1 ) firstDuration *= 0.8;
    else if ( initialPosDif == 0 ) firstDuration = 0;
    
    __weak SWRevealViewController *theSelf = self;
    if ( animated )
    {
        _enqueue( [theSelf _setFrontViewPosition:preReplacementPosition withDuration:firstDuration] );
        _enqueue( [theSelf _performTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:newFrontViewController animated:NO] );
        _enqueue( [theSelf _setFrontViewPosition:FrontViewPositionLeft withDuration:duration] );
    }
    else
    {
        _enqueue( [theSelf _performTransitionOperation:SWRevealControllerOperationReplaceFrontController withViewController:newFrontViewController animated:NO] );
    }
}


- (void)_dispatchTransitionOperation:(SWRevealControllerOperation)operation withViewController:(UIViewController *)newViewController animated:(BOOL)animated
{
    __weak SWRevealViewController *theSelf = self;
    _enqueue( [theSelf _performTransitionOperation:operation withViewController:newViewController animated:animated] );
}

- (void)setPrimaryViewController:(UIViewController*)viewController
{
    _primaryViewController = viewController;

    // These are derived from the primary view controller
    if (@available(iOS 11.0, *)) {
        // iOS 11 betas (which are still installed in some places :\) crash with
        // doesNotRecognizeSelector since this wasn't added until after the iPhone X
        // announcement.
        if ([self respondsToSelector: @selector(setNeedsUpdateOfHomeIndicatorAutoHidden)]) {
            [self setNeedsUpdateOfHomeIndicatorAutoHidden];
        }
        if ([self respondsToSelector: @selector(setNeedsUpdateOfScreenEdgesDeferringSystemGestures)]) {
            [self setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
        }
    }
}

- (UIViewController*)childViewControllerForHomeIndicatorAutoHidden
{
    return _primaryViewController;
}

- (UIViewController*)childViewControllerForScreenEdgesDeferringSystemGestures
{
    return _primaryViewController;
}

#pragma mark Animated view controller deployment and layout

// Primitive method for view controller deployment and animated layout to the given position.
- (void)_setFrontViewPosition:(FrontViewPosition)newPosition withDuration:(NSTimeInterval)duration
{
    void (^rearDeploymentCompletion)() = [self _rearViewDeploymentForNewFrontViewPosition:newPosition];
    void (^rightDeploymentCompletion)() = [self _rightViewDeploymentForNewFrontViewPosition:newPosition];
    void (^frontDeploymentCompletion)() = [self _frontViewDeploymentForNewFrontViewPosition:newPosition];
    
    void (^animations)() = ^()
    {
        // Calling this in the animation block causes the status bar to appear/dissapear in sync with our own animation
        [self setNeedsStatusBarAppearanceUpdate];
        
        // We call the layoutSubviews method on the contentView view and send a delegate, which will
        // occur inside of an animation block if any animated transition is being performed
        [_contentView layoutSubviews];
    
        if ([_delegate respondsToSelector:@selector(revealController:animateToPosition:)])
            [_delegate revealController:self animateToPosition:_frontViewPosition];
    };
    
    void (^completion)(BOOL) = ^(BOOL finished)
    {
        rearDeploymentCompletion();
        rightDeploymentCompletion();
        frontDeploymentCompletion();
        [self _dequeue];
    };
    
    if ( duration > 0.0 )
    {
        if ( _toggleAnimationType == SWRevealToggleAnimationTypeEaseOut )
        {
            [UIView animateWithDuration:duration delay:0.0
            options:UIViewAnimationOptionCurveEaseOut animations:animations completion:completion];
        }
        else
        {
            [UIView animateWithDuration:_toggleAnimationDuration delay:0.0 usingSpringWithDamping:_springDampingRatio initialSpringVelocity:1/duration
            options:0 animations:animations completion:completion];
        }
    }
    else
    {
        animations();
        completion(YES);
    }
}


// Primitive method for animated controller transition
//- (void)_performTransitionToViewController:(UIViewController*)new operation:(SWRevealControllerOperation)operation animated:(BOOL)animated
- (void)_performTransitionOperation:(SWRevealControllerOperation)operation withViewController:(UIViewController*)new animated:(BOOL)animated
{
    if ( [_delegate respondsToSelector:@selector(revealController:willAddViewController:forOperation:animated:)] )
        [_delegate revealController:self willAddViewController:new forOperation:operation animated:animated];

    UIViewController *old = nil;
    UIView *view = nil;
    
    if ( operation == SWRevealControllerOperationReplaceRearController )
        old = _rearViewController, _rearViewController = new, view = _contentView.rearView;
    
    else if ( operation == SWRevealControllerOperationReplaceFrontController )
        old = _frontViewController, _frontViewController = new, view = _contentView.frontView;
    
    else if ( operation == SWRevealControllerOperationReplaceRightController )
        old = _rightViewController, _rightViewController = new, view = _contentView.rightView;

    void (^completion)() = [self _transitionFromViewController:old toViewController:new inView:view];
    
    void (^animationCompletion)() = ^
    {
        completion();
        if ( [_delegate respondsToSelector:@selector(revealController:didAddViewController:forOperation:animated:)] )
            [_delegate revealController:self didAddViewController:new forOperation:operation animated:animated];
    
        [self _dequeue];
    };
    
    if ( animated )
    {
        id<UIViewControllerAnimatedTransitioning> animationController = nil;
    
        if ( [_delegate respondsToSelector:@selector(revealController:animationControllerForOperation:fromViewController:toViewController:)] )
            animationController = [_delegate revealController:self animationControllerForOperation:operation fromViewController:old toViewController:new];
    
        if ( !animationController )
            animationController = [[SWDefaultAnimationController alloc] initWithDuration:_replaceViewAnimationDuration];
    
        SWContextTransitionObject *transitioningObject = [[SWContextTransitionObject alloc] initWithRevealController:self containerView:view
            fromVC:old toVC:new completion:animationCompletion];
    
        if ( [animationController transitionDuration:transitioningObject] > 0 )
            [animationController animateTransition:transitioningObject];
        else
            animationCompletion();
    }
    else
    {
        animationCompletion();
    }
}


#pragma mark Position based view controller deployment

// Deploy/Undeploy of the front view controller following the containment principles. Returns a block
// that must be invoked on animation completion in order to finish deployment
- (void (^)(void))_frontViewDeploymentForNewFrontViewPosition:(FrontViewPosition)newPosition
{
    if ( (_rightViewController == nil && newPosition < FrontViewPositionLeft) ||
         (_rearViewController == nil && newPosition > FrontViewPositionLeft) )
        newPosition = FrontViewPositionLeft;
    
    BOOL positionIsChanging = (_frontViewPosition != newPosition);
    
    BOOL appear =
        (_frontViewPosition >= FrontViewPositionRightMostRemoved || _frontViewPosition <= FrontViewPositionLeftSideMostRemoved || _frontViewPosition == FrontViewPositionNone) &&
        (newPosition < FrontViewPositionRightMostRemoved && newPosition > FrontViewPositionLeftSideMostRemoved);
    
    BOOL disappear =
        (newPosition >= FrontViewPositionRightMostRemoved || newPosition <= FrontViewPositionLeftSideMostRemoved ) &&
        (_frontViewPosition < FrontViewPositionRightMostRemoved && _frontViewPosition > FrontViewPositionLeftSideMostRemoved && _frontViewPosition != FrontViewPositionNone);
    
    if ( positionIsChanging )
    {
        if ( [_delegate respondsToSelector:@selector(revealController:willMoveToPosition:)] )
            [_delegate revealController:self willMoveToPosition:newPosition];
    }
    
    _frontViewPosition = newPosition;
    
    void (^deploymentCompletion)() =
        [self _deploymentForViewController:_frontViewController inView:_contentView.frontView appear:appear disappear:disappear];
    
    void (^completion)() = ^()
    {
        deploymentCompletion();
        if ( positionIsChanging )
        {
            if ( [_delegate respondsToSelector:@selector(revealController:didMoveToPosition:)] )
                [_delegate revealController:self didMoveToPosition:newPosition];
        }
    };

    return completion;
}

// Deploy/Undeploy of the left view controller following the containment principles. Returns a block
// that must be invoked on animation completion in order to finish deployment
- (void (^)(void))_rearViewDeploymentForNewFrontViewPosition:(FrontViewPosition)newPosition
{
    if ( _presentFrontViewHierarchically )
        newPosition = FrontViewPositionRight;
    
    if ( _rearViewController == nil && newPosition > FrontViewPositionLeft )
        newPosition = FrontViewPositionLeft;

    BOOL appear = (_rearViewPosition <= FrontViewPositionLeft || _rearViewPosition == FrontViewPositionNone) && newPosition > FrontViewPositionLeft;
    BOOL disappear = newPosition <= FrontViewPositionLeft && (_rearViewPosition > FrontViewPositionLeft && _rearViewPosition != FrontViewPositionNone);
    
    if ( appear )
        [_contentView prepareRearViewForPosition:newPosition];
    
    _rearViewPosition = newPosition;
    
    return [self _deploymentForViewController:_rearViewController inView:_contentView.rearView appear:appear disappear:disappear];
}

// Deploy/Undeploy of the right view controller following the containment principles. Returns a block
// that must be invoked on animation completion in order to finish deployment
- (void (^)(void))_rightViewDeploymentForNewFrontViewPosition:(FrontViewPosition)newPosition
{
    if ( _rightViewController == nil && newPosition < FrontViewPositionLeft )
        newPosition = FrontViewPositionLeft;

    BOOL appear = (_rightViewPosition >= FrontViewPositionLeft || _rightViewPosition == FrontViewPositionNone) && newPosition < FrontViewPositionLeft ;
    BOOL disappear = newPosition >= FrontViewPositionLeft && (_rightViewPosition < FrontViewPositionLeft && _rightViewPosition != FrontViewPositionNone);
    
    if ( appear )
        [_contentView prepareRightViewForPosition:newPosition];
    
    _rightViewPosition = newPosition;
    
    return [self _deploymentForViewController:_rightViewController inView:_contentView.rightView appear:appear disappear:disappear];
}


- (void (^)(void)) _deploymentForViewController:(UIViewController*)controller inView:(UIView*)view appear:(BOOL)appear disappear:(BOOL)disappear
{
    if ( appear ) return [self _deployForViewController:controller inView:view];
    if ( disappear ) return [self _undeployForViewController:controller];
    return ^{};
}


#pragma mark Containment view controller deployment and transition

// Containment Deploy method. Returns a block to be invoked at the
// animation completion, or right after return in case of non-animated deployment.
- (void (^)(void))_deployForViewController:(UIViewController*)controller inView:(UIView*)view
{
    if ( !controller || !view )
        return ^(void){};
    
    CGRect frame = view.bounds;
    
    UIView *controllerView = controller.view;
    controllerView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    controllerView.frame = frame;
    
    if ( [controllerView isKindOfClass:[UIScrollView class]] )
    {
        BOOL adjust = controller.automaticallyAdjustsScrollViewInsets;
        
        if ( adjust )
        {
            [(id)controllerView setContentInset:UIEdgeInsetsMake(statusBarAdjustment(_contentView), 0, 0, 0)];
        }
    }
    
    [view addSubview:controllerView];
    
    void (^completionBlock)(void) = ^(void)
    {
        // nothing to do on completion at this stage
    };
    
    return completionBlock;
}

// Containment Undeploy method. Returns a block to be invoked at the
// animation completion, or right after return in case of non-animated deployment.
- (void (^)(void))_undeployForViewController:(UIViewController*)controller
{
    if (!controller)
        return ^(void){};

    // nothing to do before completion at this stage
    
    void (^completionBlock)(void) = ^(void)
    {
        [controller.view removeFromSuperview];
    };
    
    return completionBlock;
}

// Containment Transition method. Returns a block to be invoked at the
// animation completion, or right after return in case of non-animated transition.
- (void(^)(void))_transitionFromViewController:(UIViewController*)fromController toViewController:(UIViewController*)toController inView:(UIView*)view
{
    if ( fromController == toController )
        return ^(void){};
    
    if ( toController ) [self addChildViewController:toController];
    
    void (^deployCompletion)() = [self _deployForViewController:toController inView:view];
    
    [fromController willMoveToParentViewController:nil];
    
    void (^undeployCompletion)() = [self _undeployForViewController:fromController];
    
    void (^completionBlock)(void) = ^(void)
    {
        undeployCompletion() ;
        [fromController removeFromParentViewController];
        
        deployCompletion() ;
        [toController didMoveToParentViewController:self];
    };
    return completionBlock;
}

// Load any defined front/rear controllers from the storyboard
// This method is intended to be overrided in case the default behavior will not meet your needs
- (void)loadStoryboardControllers
{
    if ( self.storyboard && _rearViewController == nil )
    {
        //Try each segue separately so it doesn't break prematurely if either Rear or Right views are not used.
        @try
        {
            [self performSegueWithIdentifier:SWSegueRearIdentifier sender:nil];
        }
        @catch(NSException *exception) {}
        
        @try
        {
            [self performSegueWithIdentifier:SWSegueFrontIdentifier sender:nil];
        }
        @catch(NSException *exception) {}
        
        @try
        {
            [self performSegueWithIdentifier:SWSegueRightIdentifier sender:nil];
        }
        @catch(NSException *exception) {}
    }
}


#pragma mark state preservation / restoration

+ (UIViewController *)viewControllerWithRestorationIdentifierPath:(NSArray *)identifierComponents coder:(NSCoder*)coder
{
    SWRevealViewController* vc = nil;
    UIStoryboard* sb = [coder decodeObjectForKey:UIStateRestorationViewControllerStoryboardKey];
    
    if (sb)
    {
        vc = (SWRevealViewController*)[sb instantiateViewControllerWithIdentifier:@"SWRevealViewController"];
        vc.restorationIdentifier = [identifierComponents lastObject];
        vc.restorationClass = [SWRevealViewController class];
    }
    return vc;
}


- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
    [coder encodeDouble:_rearViewRevealWidth forKey:@"_rearViewRevealWidth"];
    [coder encodeDouble:_rearViewRevealOverdraw forKey:@"_rearViewRevealOverdraw"];
    [coder encodeDouble:_rearViewRevealDisplacement forKey:@"_rearViewRevealDisplacement"];
    [coder encodeDouble:_rightViewRevealWidth forKey:@"_rightViewRevealWidth"];
    [coder encodeDouble:_rightViewRevealOverdraw forKey:@"_rightViewRevealOverdraw"];
    [coder encodeDouble:_rightViewRevealDisplacement forKey:@"_rightViewRevealDisplacement"];
    [coder encodeBool:_bounceBackOnOverdraw forKey:@"_bounceBackOnOverdraw"];
    [coder encodeBool:_bounceBackOnLeftOverdraw forKey:@"_bounceBackOnLeftOverdraw"];
    [coder encodeBool:_stableDragOnOverdraw forKey:@"_stableDragOnOverdraw"];
    [coder encodeBool:_stableDragOnLeftOverdraw forKey:@"_stableDragOnLeftOverdraw"];
    [coder encodeBool:_presentFrontViewHierarchically forKey:@"_presentFrontViewHierarchically"];
    [coder encodeDouble:_quickFlickVelocity forKey:@"_quickFlickVelocity"];
    [coder encodeDouble:_toggleAnimationDuration forKey:@"_toggleAnimationDuration"];
    [coder encodeInteger:_toggleAnimationType forKey:@"_toggleAnimationType"];
    [coder encodeDouble:_springDampingRatio forKey:@"_springDampingRatio"];
    [coder encodeDouble:_replaceViewAnimationDuration forKey:@"_replaceViewAnimationDuration"];
    [coder encodeDouble:_frontViewShadowRadius forKey:@"_frontViewShadowRadius"];
    [coder encodeCGSize:_frontViewShadowOffset forKey:@"_frontViewShadowOffset"];
    [coder encodeDouble:_frontViewShadowOpacity forKey:@"_frontViewShadowOpacity"];
    [coder encodeObject:_frontViewShadowColor forKey:@"_frontViewShadowColor"];
    [coder encodeBool:_userInteractionStore forKey:@"_userInteractionStore"];
    [coder encodeDouble:_draggableBorderWidth forKey:@"_draggableBorderWidth"];
    [coder encodeBool:_clipsViewsToBounds forKey:@"_clipsViewsToBounds"];
    [coder encodeBool:_extendsPointInsideHit forKey:@"_extendsPointInsideHit"];
    
    [coder encodeObject:_rearViewController forKey:@"_rearViewController"];
    [coder encodeObject:_frontViewController forKey:@"_frontViewController"];
    [coder encodeObject:_rightViewController forKey:@"_rightViewController"];
    
    [coder encodeInteger:_frontViewPosition  forKey:@"_frontViewPosition"];
    
    [super encodeRestorableStateWithCoder:coder];
}


- (void)decodeRestorableStateWithCoder:(NSCoder *)coder
{
    _rearViewRevealWidth = [coder decodeDoubleForKey:@"_rearViewRevealWidth"];
    _rearViewRevealOverdraw = [coder decodeDoubleForKey:@"_rearViewRevealOverdraw"];
    _rearViewRevealDisplacement = [coder decodeDoubleForKey:@"_rearViewRevealDisplacement"];
    _rightViewRevealWidth = [coder decodeDoubleForKey:@"_rightViewRevealWidth"];
    _rightViewRevealOverdraw = [coder decodeDoubleForKey:@"_rightViewRevealOverdraw"];
    _rightViewRevealDisplacement = [coder decodeDoubleForKey:@"_rightViewRevealDisplacement"];
    _bounceBackOnOverdraw = [coder decodeBoolForKey:@"_bounceBackOnOverdraw"];
    _bounceBackOnLeftOverdraw = [coder decodeBoolForKey:@"_bounceBackOnLeftOverdraw"];
    _stableDragOnOverdraw = [coder decodeBoolForKey:@"_stableDragOnOverdraw"];
    _stableDragOnLeftOverdraw = [coder decodeBoolForKey:@"_stableDragOnLeftOverdraw"];
    _presentFrontViewHierarchically = [coder decodeBoolForKey:@"_presentFrontViewHierarchically"];
    _quickFlickVelocity = [coder decodeDoubleForKey:@"_quickFlickVelocity"];
    _toggleAnimationDuration = [coder decodeDoubleForKey:@"_toggleAnimationDuration"];
    _toggleAnimationType = [coder decodeIntegerForKey:@"_toggleAnimationType"];
    _springDampingRatio = [coder decodeDoubleForKey:@"_springDampingRatio"];
    _replaceViewAnimationDuration = [coder decodeDoubleForKey:@"_replaceViewAnimationDuration"];
    _frontViewShadowRadius = [coder decodeDoubleForKey:@"_frontViewShadowRadius"];
    _frontViewShadowOffset = [coder decodeCGSizeForKey:@"_frontViewShadowOffset"];
    _frontViewShadowOpacity = [coder decodeDoubleForKey:@"_frontViewShadowOpacity"];
    _frontViewShadowColor = [coder decodeObjectForKey:@"_frontViewShadowColor"];
    _userInteractionStore = [coder decodeBoolForKey:@"_userInteractionStore"];
    _animationQueue = [NSMutableArray array];
    _draggableBorderWidth = [coder decodeDoubleForKey:@"_draggableBorderWidth"];
    _clipsViewsToBounds = [coder decodeBoolForKey:@"_clipsViewsToBounds"];
    _extendsPointInsideHit = [coder decodeBoolForKey:@"_extendsPointInsideHit"];

    [self setRearViewController:[coder decodeObjectForKey:@"_rearViewController"]];
    [self setFrontViewController:[coder decodeObjectForKey:@"_frontViewController"]];
    [self setRightViewController:[coder decodeObjectForKey:@"_rightViewController"]];
    
    [self setFrontViewPosition:[coder decodeIntForKey: @"_frontViewPosition"]];
    
    [super decodeRestorableStateWithCoder:coder];
}


- (void)applicationFinishedRestoringState
{
    // nothing to do at this stage
}


@end


#pragma mark - UIViewController(SWRevealViewController) Category

@implementation UIViewController(SWRevealViewController)

- (SWRevealViewController*)revealViewController
{
    UIViewController *parent = self;
    Class revealClass = [SWRevealViewController class];
    while ( nil != (parent = [parent parentViewController]) && ![parent isKindOfClass:revealClass] ) {}
    return (id)parent;
}

@end


#pragma mark - SWRevealViewControllerSegueSetController segue identifiers

NSString * const SWSegueRearIdentifier = @"sw_rear";
NSString * const SWSegueFrontIdentifier = @"sw_front";
NSString * const SWSegueRightIdentifier = @"sw_right";


#pragma mark - SWRevealViewControllerSegueSetController class

@implementation SWRevealViewControllerSegueSetController

- (void)perform
{
    SWRevealControllerOperation operation = SWRevealControllerOperationNone;
    
    NSString *identifier = self.identifier;
    SWRevealViewController *rvc = self.sourceViewController;
    UIViewController *dvc = self.destinationViewController;
    
    if ( [identifier isEqualToString:SWSegueFrontIdentifier] )
        operation = SWRevealControllerOperationReplaceFrontController;
    
    else if ( [identifier isEqualToString:SWSegueRearIdentifier] )
        operation = SWRevealControllerOperationReplaceRearController;
    
    else if ( [identifier isEqualToString:SWSegueRightIdentifier] )
        operation = SWRevealControllerOperationReplaceRightController;
    
    if ( operation != SWRevealControllerOperationNone )
        [rvc _performTransitionOperation:operation withViewController:dvc animated:NO];
}

@end


#pragma mark - SWRevealViewControllerSeguePushController class

@implementation SWRevealViewControllerSeguePushController

- (void)perform
{
    SWRevealViewController *rvc = [self.sourceViewController revealViewController];
    UIViewController *dvc = self.destinationViewController;
    [rvc pushFrontViewController:dvc animated:YES];
}

@end


//#pragma mark - SWRevealViewControllerSegue Class
//
//@implementation SWRevealViewControllerSegue  // DEPRECATED
//
//- (void)perform
//{
//    if ( _performBlock )
//        _performBlock( self, self.sourceViewController, self.destinationViewController );
//}
//
//@end
//
//
//#pragma mark Storyboard support
//
//@implementation SWRevealViewController(deprecated)
//
//- (void)prepareForSegue:(SWRevealViewControllerSegue *)segue sender:(id)sender   // TO REMOVE: DEPRECATED IMPLEMENTATION
//{
//    // This method is required for compatibility with SWRevealViewControllerSegue, now deprecated.
//    // It can be simply removed when using SWRevealViewControllerSegueSetController and SWRevealViewControlerSeguePushController
//    
//    NSString *identifier = segue.identifier;
//    if ( [segue isKindOfClass:[SWRevealViewControllerSegue class]] && sender == nil )
//    {
//        if ( [identifier isEqualToString:SWSegueRearIdentifier] )
//        {
//            segue.performBlock = ^(SWRevealViewControllerSegue* rvc_segue, UIViewController* svc, UIViewController* dvc)
//            {
//                [self _setRearViewController:dvc animated:NO];
//            };
//        }
//        else if ( [identifier isEqualToString:SWSegueFrontIdentifier] )
//        {
//            segue.performBlock = ^(SWRevealViewControllerSegue* rvc_segue, UIViewController* svc, UIViewController* dvc)
//            {
//                [self _setFrontViewController:dvc animated:NO];
//            };
//        }
//        else if ( [identifier isEqualToString:SWSegueRightIdentifier] )
//        {
//            segue.performBlock = ^(SWRevealViewControllerSegue* rvc_segue, UIViewController* svc, UIViewController* dvc)
//            {
//                [self _setRightViewController:dvc animated:NO];
//            };
//        }
//    }
//}
//
//@end


