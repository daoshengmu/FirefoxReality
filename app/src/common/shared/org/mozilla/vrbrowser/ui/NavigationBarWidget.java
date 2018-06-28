/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.vrbrowser.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.geckoview.GeckoResponse;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.vrbrowser.R;
import org.mozilla.vrbrowser.SessionStore;
import org.mozilla.vrbrowser.Widget;
import org.mozilla.vrbrowser.WidgetManagerDelegate;
import org.mozilla.vrbrowser.WidgetPlacement;
import org.mozilla.vrbrowser.audio.AudioEngine;

import java.util.ArrayList;
import java.util.Arrays;

public class NavigationBarWidget extends UIWidget implements GeckoSession.NavigationDelegate, GeckoSession.ProgressDelegate, GeckoSession.ContentDelegate, WidgetManagerDelegate.Listener {
    private static final String LOGTAG = "VRB";
    private AudioEngine mAudio;
    private NavigationBarButton mBackButton;
    private NavigationBarButton mForwardButton;
    private NavigationBarButton mReloadButton;
    private NavigationBarButton mHomeButton;
    private NavigationURLBar mURLBar;
    private ViewGroup mNavigationContainer;
    private ViewGroup mFocusModeContainer;
    private ViewGroup mResizeModeContainer;
    private BrowserWidget mBrowserWidget;
    private boolean mIsLoading;
    private boolean mIsInFocusMode;
    private boolean mIsResizing;
    private boolean mFocusDueToFullScreen;
    private Runnable mFocusBackHandler;
    private Runnable mResizeBackHandler;
    private NavigationBarButton mFocusEnterbutton;
    private NavigationBarButton mFocusExitButton;
    private NavigationBarButton mResizeEnterButton;
    private NavigationBarButton mResizeExitButton;
    private NavigationBarTextButton mPreset0;
    private NavigationBarTextButton mPreset1;
    private NavigationBarTextButton mPreset2;
    private NavigationBarTextButton mPreset3;
    private ArrayList<CustomUIButton> mButtons;

    public NavigationBarWidget(Context aContext) {
        super(aContext);
        initialize(aContext);
    }

    public NavigationBarWidget(Context aContext, AttributeSet aAttrs) {
        super(aContext, aAttrs);
        initialize(aContext);
    }

    public NavigationBarWidget(Context aContext, AttributeSet aAttrs, int aDefStyle) {
        super(aContext, aAttrs, aDefStyle);
        initialize(aContext);
    }

    private void initialize(Context aContext) {
        inflate(aContext, R.layout.navigation_bar, this);
        mAudio = AudioEngine.fromContext(aContext);
        mBackButton = findViewById(R.id.backButton);
        mForwardButton = findViewById(R.id.forwardButton);
        mReloadButton = findViewById(R.id.reloadButton);
        mHomeButton = findViewById(R.id.homeButton);
        mURLBar = findViewById(R.id.urlBar);
        mNavigationContainer = findViewById(R.id.navigationBarContainer);
        mFocusModeContainer = findViewById(R.id.focusModeContainer);
        mResizeModeContainer = findViewById(R.id.resizeModeContainer);
        mFocusBackHandler = new Runnable() {
            @Override
            public void run() {
                exitFocusMode();
            }
        };

        mResizeBackHandler = new Runnable() {
            @Override
            public void run() {
                exitResizeMode(true);
            }
        };

        mBackButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                SessionStore.get().goBack();
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.BACK);
                }
            }
        });

        mForwardButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                SessionStore.get().goForward();
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mReloadButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mIsLoading) {
                    SessionStore.get().stop();
                } else {
                    SessionStore.get().reload();
                }
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mHomeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                SessionStore.get().loadUri(SessionStore.DEFAULT_URL);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });


        mFocusEnterbutton = findViewById(R.id.focusEnterButton);
        mFocusExitButton = findViewById(R.id.focusExitButton);
        mResizeEnterButton = findViewById(R.id.resizeEnterButton);
        mResizeExitButton = findViewById(R.id.resizeExitButton);
        mPreset0 = findViewById(R.id.resizePreset0);
        mPreset1 = findViewById(R.id.resizePreset1);
        mPreset2 = findViewById(R.id.resizePreset2);
        mPreset3 = findViewById(R.id.resizePreset3);


        mFocusEnterbutton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                enterFocusMode();
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mFocusExitButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                exitFocusMode();
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mResizeEnterButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                enterResizeMode();
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mResizeExitButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                exitResizeMode(true);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mPreset0.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                setResizePreset(0.5f);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mPreset1.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                setResizePreset(1.0f);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mPreset2.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                setResizePreset(2.0f);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mPreset3.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                setResizePreset(3.0f);
                if (mAudio != null) {
                    mAudio.playSound(AudioEngine.Sound.CLICK);
                }
            }
        });

        mButtons = new ArrayList<>();
        mButtons.addAll(Arrays.<CustomUIButton>asList(
                mBackButton, mForwardButton, mReloadButton, mHomeButton,
                mFocusEnterbutton, mFocusExitButton, mResizeEnterButton, mResizeExitButton,
                mPreset0, mPreset1, mPreset2, mPreset3));

        SessionStore.get().addNavigationListener(this);
        SessionStore.get().addProgressListener(this);
        SessionStore.get().addContentListener(this);
        mWidgetManager.addListener(this);
    }

    @Override
    public void releaseWidget() {
        mWidgetManager.removeListener(this);
        SessionStore.get().removeNavigationListener(this);
        SessionStore.get().removeProgressListener(this);
        SessionStore.get().removeContentListener(this);
        mBrowserWidget = null;
        super.releaseWidget();
    }

    public void setURLText(String aText) {
        mURLBar.setURLText(aText);
    }

    @Override
    void initializeWidgetPlacement(WidgetPlacement aPlacement) {
        aPlacement.width = WidgetPlacement.dpDimension(getContext(), R.dimen.navigation_bar_width);
        aPlacement.worldWidth = WidgetPlacement.floatDimension(getContext(), R.dimen.browser_world_width);
        aPlacement.height = WidgetPlacement.dpDimension(getContext(), R.dimen.navigation_bar_height);
        aPlacement.anchorX = 0.5f;
        aPlacement.anchorY = 1.0f;
        aPlacement.parentAnchorX = 0.5f;
        aPlacement.parentAnchorY = 0.0f;
        aPlacement.translationY = -20;
        aPlacement.opaque = false;
    }

    public void setBrowserWidget(BrowserWidget aWidget) {
        if (aWidget != null) {
            mWidgetPlacement.parentHandle = aWidget.getHandle();
        }
        mBrowserWidget = aWidget;
    }

    private void enterFocusMode() {
        if (mIsInFocusMode) {
            return;
        }
        mIsInFocusMode = true;
        AnimationHelper.fadeIn(mFocusModeContainer, AnimationHelper.FADE_ANIMATION_DURATION);
        AnimationHelper.fadeOut(mNavigationContainer, 0);
        mWidgetManager.fadeOutWorld();

        mWidgetPlacement.anchorX = 1.0f;
        mWidgetPlacement.parentAnchorX = 1.0f;
        mWidgetManager.updateWidget(this);
        mWidgetManager.pushBackHandler(mFocusBackHandler);
    }

    private void exitFocusMode() {
        if (!mIsInFocusMode) {
            return;
        }
        mIsInFocusMode = false;
        AnimationHelper.fadeIn(mNavigationContainer, AnimationHelper.FADE_ANIMATION_DURATION);
        AnimationHelper.fadeOut(mFocusModeContainer, 0);
        mWidgetManager.fadeInWorld();
        setResizePreset(1.0f);

        mWidgetPlacement.anchorX = 0.5f;
        mWidgetPlacement.parentAnchorX = 0.5f;
        mWidgetManager.updateWidget(this);
        mWidgetManager.popBackHandler(mFocusBackHandler);

        if (SessionStore.get().isInFullScreen()) {
            SessionStore.get().exitFullScreen();
        }
    }

    private void enterResizeMode() {
        if (mIsResizing) {
            return;
        }
        mIsResizing = true;
        mWidgetManager.startWidgetResize(mBrowserWidget);
        AnimationHelper.fadeIn(mResizeModeContainer, AnimationHelper.FADE_ANIMATION_DURATION);
        AnimationHelper.fadeOut(mFocusModeContainer, 0);
        mWidgetManager.pushBackHandler(mResizeBackHandler);
    }

    private void exitResizeMode(boolean aCommitChanges) {
        if (!mIsResizing) {
            return;
        }
        mIsResizing = false;
        mWidgetManager.finishWidgetResize(mBrowserWidget);
        AnimationHelper.fadeIn(mFocusModeContainer, AnimationHelper.FADE_ANIMATION_DURATION);
        AnimationHelper.fadeOut(mResizeModeContainer, 0);
        mWidgetManager.popBackHandler(mResizeBackHandler);
    }

    private void setResizePreset(float aPreset) {
        float worldWidth = WidgetPlacement.floatDimension(getContext(), R.dimen.browser_world_width);
        float aspect = (float) WidgetPlacement.pixelDimension(getContext(), R.dimen.browser_width_pixels) / (float) WidgetPlacement.pixelDimension(getContext(), R.dimen.browser_height_pixels);
        float worldHeight = worldWidth / aspect;
        float area = worldWidth * worldHeight * aPreset;

        float targetWidth = (float) Math.sqrt(area * aspect);
        float targetHeight = (float) Math.sqrt(area / aspect);

        mBrowserWidget.handleResizeEvent(targetWidth, targetHeight);
    }

    @Override
    public void onNewSession(GeckoSession aSession, String aUrl, GeckoResponse<GeckoSession> aResponse) {
        aResponse.respond(null);
    }

    public void release() {
        SessionStore.get().removeNavigationListener(this);
        SessionStore.get().removeProgressListener(this);
    }

    @Override
    public void onLocationChange(GeckoSession session, String url) {
        if (mURLBar != null) {
            Log.e(LOGTAG, "Got location change: " + url);
            mURLBar.setURL(url);
            mReloadButton.setEnabled(true);
        }
    }

    @Override
    public void onCanGoBack(GeckoSession aSession, boolean canGoBack) {
        if (mBackButton != null) {
            Log.e(LOGTAG, "Got onCanGoBack: " + (canGoBack ? "TRUE" : "FALSE"));
            mBackButton.setEnabled(canGoBack);
            mBackButton.setClickable(canGoBack);
        }
    }

    @Override
    public void onCanGoForward(GeckoSession aSession, boolean canGoForward) {
        if (mForwardButton != null) {
            Log.e(LOGTAG, "Got onCanGoForward: " + (canGoForward ? "TRUE" : "FALSE"));
            mForwardButton.setEnabled(canGoForward);
            mForwardButton.setClickable(canGoForward);
        }
    }

    @Override
    public void onLoadRequest(GeckoSession aSession, String aUri, int target, int flags, GeckoResponse<Boolean> aResponse) {
        if (mURLBar != null) {
            Log.e(LOGTAG, "Got onLoadUri: " + aUri);
            mURLBar.setURL(aUri);
        }
        aResponse.respond(null);
    }

    // Progress Listener
    @Override
    public void onPageStart(GeckoSession aSession, String aUri) {
        if (mURLBar != null) {
            Log.e(LOGTAG, "Got onPageStart: " + aUri);
            mURLBar.setURL(aUri);
        }
        mIsLoading = true;
        mURLBar.setIsLoading(true);
        if (mReloadButton != null) {
            mReloadButton.setImageResource(R.drawable.ic_icon_exit);
        }
    }

    @Override
    public void onPageStop(GeckoSession aSession, boolean b) {
        mIsLoading = false;
        mURLBar.setIsLoading(false);
        if (mReloadButton != null) {
            mReloadButton.setImageResource(R.drawable.ic_icon_reload);
        }
    }

    @Override
    public void onSecurityChange(GeckoSession geckoSession, SecurityInformation securityInformation) {
        if (mURLBar != null) {
            Log.e(LOGTAG, "Got onSecurityChange: " + securityInformation.isSecure);
            mURLBar.setIsInsecure(!securityInformation.isSecure);
        }
    }

    public void setPrivateBrowsingEnabled(boolean isEnabled) {
        mURLBar.setPrivateBrowsingEnabled(isEnabled);

        if (isEnabled) {
            for (CustomUIButton button : mButtons) {
                button.setBackground(getContext().getDrawable(R.drawable.main_button_private));
                button.setTintColorList(R.drawable.main_button_icon_color_private);
            }

        } else {
            for (CustomUIButton button : mButtons) {
                button.setBackground(getContext().getDrawable(R.drawable.main_button));
                button.setTintColorList(R.drawable.main_button_icon_color);
            }
        }

    }

    // Content delegate

    @Override
    public void onTitleChange(GeckoSession session, String title) {

    }

    @Override
    public void onFocusRequest(GeckoSession session) {

    }

    @Override
    public void onCloseRequest(GeckoSession session) {

    }

    @Override
    public void onFullScreen(GeckoSession session, boolean aFullScreen) {
        if (aFullScreen) {
            if (!mIsInFocusMode) {
                mFocusDueToFullScreen = true;
                enterFocusMode();
            }
            if (mIsResizing) {
                exitResizeMode(false);
            }
            // Set default fullscreen size
            setResizePreset(2.0f);

        } else {
            if (mFocusDueToFullScreen) {
                mFocusDueToFullScreen = false;
                exitFocusMode();
            }
        }
    }

    @Override
    public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, int elementType, String elementSrc) {

    }

    @Override
    public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo response) {

    }

    @Override
    public void onCrash(GeckoSession session) {

    }

    // WidgetManagerDelegate.Listener
    @Override
    public void onWidgetUpdate(Widget aWidget) {
        if (aWidget != mBrowserWidget) {
            return;
        }

        // Browser window may have been resized, adjust the navigation bar
        float targetWidth = aWidget.getPlacement().worldWidth;
        float defaultWidth = WidgetPlacement.floatDimension(getContext(), R.dimen.browser_world_width);
        targetWidth = Math.max(defaultWidth, targetWidth);
        // targetWidth = Math.min((targetWidth, defaultWidth * 2.0f);

        float ratio = targetWidth / defaultWidth;
        mWidgetPlacement.worldWidth = targetWidth;
        mWidgetPlacement.width = (int) (WidgetPlacement.dpDimension(getContext(), R.dimen.navigation_bar_width) * ratio);
        mWidgetManager.updateWidget(this);
    }
}
