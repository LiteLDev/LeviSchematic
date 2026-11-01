#pragma once

class BlockSource;
class Dimension;

namespace levischematic::app {
class AppKernel;






void refreshProjectionState(AppKernel& session, BlockSource* source);





void refreshProjectionStateForDimension(AppKernel& session, Dimension* dimension);




void refreshCurrentClientProjectionState(AppKernel& session);
} 
