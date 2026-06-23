#include "PageManager.h"

#define IS_PAGE(page)   ((page)<(MaxPage))

#ifndef NULL
#   define NULL 0
#endif

/**
  * @brief  Initialize the page scheduler.
  * @param  pageMax: Maximum number of pages.
  * @param  pageStackSize: Maximum page stack depth.
  * @retval None.
  */
PageManager::PageManager(uint8_t pageMax, uint8_t pageStackSize)
{
    MaxPage = pageMax;
    NewPage = NULL;
    OldPage = UINT8_MAX;
    IsPageBusy = false;

    /* Allocate and clear the page table. */
    PageList = new PageList_TypeDef[MaxPage];
    for(uint8_t page = 0; page < MaxPage; page++)
    {
        PageClear(page);
    }
    
    /* Initialize the page stack. */
    PageStackSize = pageStackSize;
    PageStack = new uint8_t[PageStackSize];
    PageStackClear();
}

/**
  * @brief  Destroy the page scheduler.
  * @param  None.
  * @retval None.
  */
PageManager::~PageManager()
{
    delete[] PageList;
    delete[] PageStack;
}

/**
  * @brief  Clear one registered page.
  * @param  pageID: Page ID.
  * @retval true on success, false on failure.
  */
bool PageManager::PageClear(uint8_t pageID)
{
    if(!IS_PAGE(pageID))
        return false;

    PageList[pageID].SetupCallback = NULL;
    PageList[pageID].LoopCallback = NULL;
    PageList[pageID].ExitCallback = NULL;
    PageList[pageID].EventCallback = NULL;

    return true;
}

/**
  * @brief  Register a page with setup, loop, exit, and event callbacks.
  * @param  pageID: Page ID.
  * @param  setupCallback: Setup callback.
  * @param  loopCallback: Loop callback.
  * @param  exitCallback: Exit callback.
  * @param  eventCallback: Event callback.
  * @retval true on success, false on failure.
  */
bool PageManager::PageRegister(
    uint8_t pageID,
    CallbackFunction_t setupCallback,
    CallbackFunction_t loopCallback,
    CallbackFunction_t exitCallback,
    EventFunction_t eventCallback
)
{
    if(!IS_PAGE(pageID))
        return false;

    PageList[pageID].SetupCallback = setupCallback;
    PageList[pageID].LoopCallback = loopCallback;
    PageList[pageID].ExitCallback = exitCallback;
    PageList[pageID].EventCallback = eventCallback;
    return true;
}

/**
  * @brief  Dispatch an event to the current page.
  * @param  obj: Event source object.
  * @param  event: Event ID.
  * @retval None.
  */
void PageManager::PageEventTransmit(void* obj, int event)
{
    /* Dispatch the event to the current page. */
    if(PageList[NowPage].EventCallback != NULL)
        PageList[NowPage].EventCallback(obj, event);
}

/**
  * @brief  Switch to the selected page.
  * @param  pageID: Page ID.
  * @retval None.
  */
void PageManager::PageChangeTo(uint8_t pageID)
{
    if(!IS_PAGE(pageID))
        return;
    
    /* Switch only when the page manager is idle. */
    if(!IsPageBusy)
    {
        /* Save the next page ID. */
        NextPage = NewPage = pageID;

        /* Mark the page manager as busy. */
        IsPageBusy = true;
    }
}

/**
  * @brief  Push one page onto the stack and switch to it.
  * @param  pageID: Page ID.
  * @retval true on success, false on failure.
  */
bool PageManager::PagePush(uint8_t pageID)
{
    if(!IS_PAGE(pageID))
        return false;
    
    /* Reject pushes while the page manager is busy. */
    if(IsPageBusy)
       return false; 
    
    /* Prevent stack overflow. */
    if(PageStackTop >= PageStackSize - 1)
        return false;
    
    /* Prevent duplicate consecutive pages. */
    if(pageID == PageStack[PageStackTop])
        return false;

    /* Move the stack top up. */
    PageStackTop++;
    
    /* Store the page on the stack. */
    PageStack[PageStackTop] = pageID;
    
    /* Switch to the new stack top. */
    PageChangeTo(PageStack[PageStackTop]);
    
    return true;
}

/**
  * @brief  Pop the current page and switch to the previous page.
  * @param  None.
  * @retval true on success, false on failure.
  */
bool PageManager::PagePop()
{
    /* Reject pops while the page manager is busy. */
    if(IsPageBusy)
       return false; 
    
    /* Do not pop the base page. */
    if(PageStackTop == 0)
        return false;
    
    /* Clear the current stack slot. */
    PageStack[PageStackTop] = 0;
    
    /* Move the stack top down. */
    PageStackTop--;
    
    /* Switch to the new stack top. */
    PageChangeTo(PageStack[PageStackTop]);
    
    return true;
}

/**
  * @brief  Clear the page stack.
  * @param  None.
  * @retval None.
  */
void PageManager::PageStackClear()
{
    /* Reject stack clears while the page manager is busy. */
    if(IsPageBusy)
       return; 
    
    /* Clear all stack entries. */
    for(uint8_t i = 0; i < PageStackSize; i++)
    {
        PageStack[i] = 0;
    }
    /* Reset the stack top. */
    PageStackTop = 0;
}

/**
  * @brief  Run the page scheduler state machine.
  * @param  None.
  * @retval None.
  */
void PageManager::Running()
{
    /* Handle page switching. */
    if(NewPage != OldPage)
    {
        /* Mark the page manager as busy. */
        IsPageBusy = true;

        /* Trigger the old page exit callback. */
        if(IS_PAGE(OldPage) && PageList[OldPage].ExitCallback != NULL)
            PageList[OldPage].ExitCallback(NewPage>=OldPage?1:-1);
        
        /* Remember the old page. */
        LastPage = OldPage;
        
        /* Mark the new page as current. */
        NowPage = NewPage;

        /* Trigger the new page setup callback. */
        if(PageList[NewPage].SetupCallback != NULL && IS_PAGE(NewPage))
            PageList[NewPage].SetupCallback(NewPage>=OldPage?1:-1);

        /* Setup finished; store the current page as the old page. */
        OldPage = NewPage;
    }
    else
    {
        /* Mark the page manager as idle while running loop callbacks. */
        IsPageBusy = false;
        
        /* Trigger the current page loop callback. */
        if(PageList[NowPage].LoopCallback != NULL && IS_PAGE(NowPage))
            PageList[NowPage].LoopCallback(0);
    }
}
