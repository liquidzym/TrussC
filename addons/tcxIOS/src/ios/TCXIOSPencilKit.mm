#include "TCXIOSBridgeSupport.h"

#import <ARKit/ARKit.h>
#import <Contacts/Contacts.h>
#import <ContactsUI/ContactsUI.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreImage/CoreImage.h>
#import <CoreML/CoreML.h>
#import <GameController/GameController.h>
#import <MultipeerConnectivity/MultipeerConnectivity.h>
#import <PencilKit/PencilKit.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>
#import <Vision/Vision.h>

#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

@interface TCXIOSPencilViewController : UIViewController
@property(nonatomic, strong) PKCanvasView* canvasView;
@property(nonatomic, strong) PKToolPicker* toolPicker;
@property(nonatomic, assign) BOOL showToolPicker;
@end

@implementation TCXIOSPencilViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.canvasView = [[PKCanvasView alloc] initWithFrame:self.view.bounds];
    self.canvasView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.canvasView.backgroundColor = UIColor.whiteColor;
    [self.view addSubview:self.canvasView];
    if (self.showToolPicker) {
        self.toolPicker = [[PKToolPicker alloc] init];
        [self.toolPicker addObserver:self.canvasView];
        [self.toolPicker setVisible:YES forFirstResponder:self.canvasView];
        [self.canvasView becomeFirstResponder];
    }
}

@end

namespace {

TCXIOSPencilViewController* gPencilController = nil;

NSMutableDictionary<NSString*, SKProduct*>* TCXIOSStoreProductCache() {
    static NSMutableDictionary<NSString*, SKProduct*>* products = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        products = [NSMutableDictionary dictionary];
    });
    return products;
}

tcx::ios::StoreTransactionState TCXIOSStoreTransactionState(SKPaymentTransactionState state) {
    switch (state) {
        case SKPaymentTransactionStatePurchasing: return tcx::ios::StoreTransactionState::Purchasing;
        case SKPaymentTransactionStatePurchased: return tcx::ios::StoreTransactionState::Purchased;
        case SKPaymentTransactionStateFailed: return tcx::ios::StoreTransactionState::Failed;
        case SKPaymentTransactionStateRestored: return tcx::ios::StoreTransactionState::Restored;
        case SKPaymentTransactionStateDeferred: return tcx::ios::StoreTransactionState::Deferred;
    }
    return tcx::ios::StoreTransactionState::Unknown;
}

tcx::ios::StoreTransactionUpdate TCXIOSStoreTransactionUpdate(SKPaymentTransaction* transaction) {
    tcx::ios::StoreTransactionUpdate update;
    update.productIdentifier = TCXIOSStr(transaction.payment.productIdentifier);
    update.transactionIdentifier = TCXIOSStr(transaction.transactionIdentifier);
    update.state = TCXIOSStoreTransactionState(transaction.transactionState);
    update.errorMessage = TCXIOSStr(transaction.error.localizedDescription);
    return update;
}

} // namespace

namespace tcx::ios::detail {
void platformPresentPencilCanvas(const PencilCanvasRequest& request, Completion<void> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* presenter = TCXIOSPresenter();
        if (!presenter) {
            TCXIOSFinishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidState, "No active presenter for PencilKit.", 0}));
            return;
        }
        gPencilController = [[TCXIOSPencilViewController alloc] init];
        gPencilController.showToolPicker = request.showToolPicker;
        gPencilController.modalPresentationStyle = UIModalPresentationFullScreen;
        [presenter presentViewController:gPencilController animated:YES completion:^{
            TCXIOSFinishVoid(std::move(done), Result<void>::success());
        }];
    });
}

void platformDismissPencilCanvas() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gPencilController dismissViewControllerAnimated:YES completion:nil];
        gPencilController = nil;
    });
}

Result<PencilDrawingData> platformCapturePencilDrawing() {
    if (!gPencilController || !gPencilController.canvasView) {
        return Result<PencilDrawingData>::failure({ErrorCode::InvalidState, "Pencil canvas is not presented.", 0});
    }
    PKDrawing* drawing = gPencilController.canvasView.drawing;
    NSData* drawingData = drawing.dataRepresentation;
    CGRect bounds = gPencilController.canvasView.bounds;
    CGFloat scale = UIScreen.mainScreen.scale;
    UIImage* image = [drawing imageFromRect:bounds scale:scale];
    NSData* png = UIImagePNGRepresentation(image);
    PencilDrawingData out;
    out.data = TCXIOSBytes(drawingData);
    out.png = TCXIOSBytes(png);
    out.pixelWidth = static_cast<int>(bounds.size.width * scale);
    out.pixelHeight = static_cast<int>(bounds.size.height * scale);
    return Result<PencilDrawingData>::success(std::move(out));
}

void platformClearPencilCanvas() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (gPencilController.canvasView) {
            gPencilController.canvasView.drawing = [[PKDrawing alloc] init];
        }
    });
}

} // namespace tcx::ios::detail
